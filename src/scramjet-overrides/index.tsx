// SPDX-License-Identifier: AGPL-3.0-only
// Obermon optimized hosted entrypoint derived from Mercury Workshop Scramjet.

import { defaultConfigDev } from "@mercuryworkshop/scramjet";
import { Controller } from "@mercuryworkshop/scramjet-controller";
import type { ProxyTransport } from "@mercuryworkshop/proxy-transports";
import { HttpCachePlugin } from "@mercuryworkshop/scramjet-utils";
import { demoSettingsStore } from "./store";
import { SharedTransportClient } from "./shared-transport-client";

const initialMount = document.getElementById("app");
if (!initialMount) throw new Error("Missing Scramjet application mount point");

let mountPoint: HTMLElement = initialMount;
let controller: InstanceType<typeof Controller>;
const cachePlugin = new HttpCachePlugin();

type TransportConstructor = new (options: { wisp: string }) => ProxyTransport;

const pageUrl = new URL(location.href);
const transportPartition = pageUrl.searchParams.get("obermon_partition") ?? "";
const initialDestination = pageUrl.searchParams.get("goto");
const requestedParameter = pageUrl.searchParams.get("tool");
const requestedTool =
	requestedParameter === "requests" ||
	requestedParameter === "playground" ||
	requestedParameter === "settings"
		? requestedParameter
		: "browser";
const sharedClients = new Set<SharedTransportClient>();

// Begin fetching only the selected surface immediately. Vite emits these as
// separate chunks, so normal browsing does not parse Monaco, request tooling,
// playground code, or settings UI.
const browserModulePromise =
	requestedTool === "browser" || requestedTool === "requests"
		? import("./pages/BrowserView")
		: null;
const requestsModulePromise =
	requestedTool === "requests" ? import("./pages/RequestViewer") : null;
const playgroundModulePromise =
	requestedTool === "playground" ? import("./pages/Playground") : null;
const settingsModulePromise =
	requestedTool === "settings" ? import("./pages/SettingsPage") : null;

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

function getSessionWispUrl(configured: string): string {
	if (!transportPartition) return configured;
	try {
		const url = new URL(configured, location.href);
		const isLocalEngine =
			(url.hostname === "127.0.0.1" || url.hostname === "localhost") &&
			(url.port === "4142" || (!url.port && url.protocol === "ws:"));
		if (!isLocalEngine) return configured;
		url.searchParams.set("partition", transportPartition);
		return url.href;
	} catch {
		return configured;
	}
}

async function loadTransportConstructor(
	transport: string
): Promise<TransportConstructor> {
	if (transport === "epoxy") {
		const module = await import("@mercuryworkshop/epoxy-transport");
		return module.default as unknown as TransportConstructor;
	}
	const module = await import("@mercuryworkshop/libcurl-transport");
	return module.default as unknown as TransportConstructor;
}

async function createTransport(): Promise<ProxyTransport> {
	const selected = demoSettingsStore.transport;
	const wisp = getSessionWispUrl(demoSettingsStore.wispUrl);

	// A SharedWorker is scoped to the browser profile's storage partition. All
	// normal Obermon tabs in that profile therefore share the same initialized
	// transport and Wisp connection pool, while incognito remains isolated.
	if (typeof SharedWorker !== "undefined" && transportPartition) {
		const shared = new SharedTransportClient({
			transport: selected,
			wisp,
			partition: transportPartition,
		});
		sharedClients.add(shared);
		try {
			await shared.init();
			return shared;
		} catch (error) {
			sharedClients.delete(shared);
			shared.dispose();
			console.warn("Shared transport unavailable; using direct transport", error);
		}
	}

	const Transport = await loadTransportConstructor(selected);
	const direct = new Transport({ wisp });
	if (!direct.ready) await direct.init();
	return direct;
}

const initialTransportPromise = createTransport();

export async function getTransport(): Promise<ProxyTransport> {
	return createTransport();
}

async function preconnectDestination(): Promise<void> {
	if (requestedTool !== "browser" || !initialDestination || !transportPartition) {
		return;
	}

	const endpoint = new URL("/preconnect", location.origin);
	endpoint.searchParams.set("destination", initialDestination);
	endpoint.searchParams.set("partition", transportPartition);
	const abortController = new AbortController();
	const timeout = setTimeout(() => abortController.abort(), 350);
	try {
		const response = await fetch(endpoint, {
			method: "POST",
			cache: "no-store",
			credentials: "omit",
			signal: abortController.signal,
		});
		if (!response.ok) {
			throw new Error(`Preconnect failed: ${response.status}`);
		}
	} catch (error) {
		// Preconnect is speculative. Failure and timeout never block navigation.
		console.debug("Obermon destination preconnect skipped", error);
	} finally {
		clearTimeout(timeout);
	}
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
	const registrationPromise = navigator.serviceWorker.register("./sw.js");
	// Fire the connection hint beside the real startup work. The request has its
	// own 350 ms budget and is not part of the controller's critical path.
	void preconnectDestination();
	const [registration, transport] = await Promise.all([
		registrationPromise,
		initialTransportPromise,
	]);
	const serviceworker = await resolveServiceWorker(registration);
	setStatus("Connecting transport…");
	controller = new Controller({
		serviceworker,
		transport,
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

	if (requestedTool === "requests") {
		const [{ default: BrowserView, browserState }, { default: RequestViewer }] =
			await Promise.all([browserModulePromise!, requestsModulePromise!]);
		const bootstrap = document.createElement("div");
		bootstrap.className = "obermon-frame-bootstrap";
		bootstrap.append(<BrowserView active={false} />);
		host.append(bootstrap);
		await waitForBrowserFrame(browserState);
		host.append(<RequestViewer active={true} />);
		return;
	}

	if (requestedTool === "playground") {
		const { default: PlaygroundView } = await playgroundModulePromise!;
		host.append(<PlaygroundView active={true} />);
		return;
	}

	if (requestedTool === "settings") {
		const { default: SettingsView } = await settingsModulePromise!;
		host.append(<SettingsView />);
		return;
	}

	const { default: BrowserView } = await browserModulePromise!;
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

addEventListener("pagehide", (event: PageTransitionEvent) => {
	// A persisted page is entering Chromium's back/forward cache. Keeping the
	// MessagePorts alive lets the restored page resume without rebuilding the
	// transport and controller. Real unloads release all per-page handles.
	if (event.persisted) return;
	for (const shared of sharedClients) shared.dispose();
	sharedClients.clear();
});

void bootstrap();
export { controller, cachePlugin };
