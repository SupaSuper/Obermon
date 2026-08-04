// SPDX-License-Identifier: AGPL-3.0-only
// Obermon optimized hosted entrypoint derived from Mercury Workshop Scramjet.

import { defaultConfigDev } from "@mercuryworkshop/scramjet";
import { Controller } from "@mercuryworkshop/scramjet-controller";
import { HttpCachePlugin } from "@mercuryworkshop/scramjet-utils";
import { demoSettingsStore } from "./store";

const initialMount = document.getElementById("app");
if (!initialMount) throw new Error("Missing Scramjet application mount point");

let mountPoint: HTMLElement = initialMount;
let controller: InstanceType<typeof Controller>;
const cachePlugin = new HttpCachePlugin();

const HOST_STYLE = `
html,body{width:100%;height:100%;margin:0;overflow:hidden;background:#000;color-scheme:dark}
#app,.obermon-host{width:100%;height:100%;min-width:0;min-height:0;display:flex;overflow:hidden;background:#000;contain:layout paint style}
.obermon-host>*{min-width:0;min-height:0}
.obermon-frame-bootstrap{position:fixed;left:-10000px;top:0;width:1px;height:1px;overflow:hidden;opacity:0;pointer-events:none;contain:strict}
.obermon-startup{display:grid;place-items:center;font:13px ui-sans-serif,system-ui,sans-serif;color:#9ca3af}
`;

function installHostStyle(): void {
	if (document.getElementById("obermon-host-style")) return;
	const style = document.createElement("style");
	style.id = "obermon-host-style";
	style.textContent = HOST_STYLE;
	document.head.append(style);
}

function setStatus(message: string): void {
	if (mountPoint.textContent !== message) mountPoint.textContent = message;
	mountPoint.classList.add("obermon-startup");
}

export async function getTransport() {
	const wisp = demoSettingsStore.wispUrl;
	if (demoSettingsStore.transport === "epoxy") {
		const { default: EpoxyClient } = await import(
			"@mercuryworkshop/epoxy-transport"
		);
		return new EpoxyClient({ wisp });
	}

	const { default: LibcurlClient } = await import(
		"@mercuryworkshop/libcurl-transport"
	);
	return new LibcurlClient({ wisp });
}

async function resolveServiceWorker(
	registration: ServiceWorkerRegistration,
	timeoutMs = 10000
): Promise<ServiceWorker> {
	const immediate = navigator.serviceWorker.controller ?? registration.active;
	if (immediate) return immediate;

	const timeout = new Promise<null>((resolve) =>
		setTimeout(() => resolve(null), timeoutMs)
	);
	const readyRegistration = await Promise.race([
		navigator.serviceWorker.ready,
		timeout,
	]);
	const worker =
		navigator.serviceWorker.controller ??
		registration.active ??
		readyRegistration?.active;
	if (!worker) throw new Error("No active Scramjet service worker");
	return worker;
}

async function initializeController(): Promise<void> {
	setStatus("Starting Scramjet…");
	const registration = await navigator.serviceWorker.register("./sw.js");
	const serviceworker = await resolveServiceWorker(registration);
	setStatus("Connecting transport…");
	controller = new Controller({
		serviceworker,
		transport: await getTransport(),
		scramjetConfig: defaultConfigDev,
	});
	await controller.wait();
}

function createHost(): HTMLElement {
	const host = document.createElement("main");
	host.className = "obermon-host";
	mountPoint.replaceWith(host);
	mountPoint = host;
	return host;
}

async function waitForBrowserFrame(
	state: { frame: unknown },
	timeoutMs = 10000
): Promise<void> {
	const deadline = performance.now() + timeoutMs;
	while (!state.frame) {
		if (performance.now() >= deadline) {
			throw new Error("Scramjet browser frame did not initialize");
		}
		await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
	}
}

async function mountSelectedView(): Promise<void> {
	const host = createHost();
	const requested = new URL(location.href).searchParams.get("tool");

	if (requested === "requests") {
		const [{ default: BrowserView, browserState }, { default: RequestViewer }] =
			await Promise.all([
				import("./pages/BrowserView"),
				import("./pages/RequestViewer"),
			]);
		const bootstrap = document.createElement("div");
		bootstrap.className = "obermon-frame-bootstrap";
		bootstrap.append(<BrowserView active={false} />);
		host.append(bootstrap);
		await waitForBrowserFrame(browserState);
		host.append(<RequestViewer active={true} />);
		return;
	}

	if (requested === "playground") {
		const { default: PlaygroundView } = await import("./pages/Playground");
		host.append(<PlaygroundView active={true} />);
		return;
	}

	if (requested === "settings") {
		const { default: SettingsView } = await import("./pages/SettingsPage");
		host.append(<SettingsView />);
		return;
	}

	const { default: BrowserView } = await import("./pages/BrowserView");
	host.append(<BrowserView active={true} />);
}

async function bootstrap(): Promise<void> {
	installHostStyle();
	try {
		await initializeController();
		await mountSelectedView();
	} catch (error) {
		console.error("Obermon Scramjet startup failed", error);
		mountPoint.replaceChildren(
			document.createTextNode(
				error instanceof Error
					? `Scramjet failed to start: ${error.message}`
					: "Scramjet failed to start."
			)
		);
	}
}

void bootstrap();
export { controller, cachePlugin };
