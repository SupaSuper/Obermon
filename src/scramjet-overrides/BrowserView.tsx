// SPDX-License-Identifier: AGPL-3.0-only
// Obermon optimized browser surface derived from Mercury Workshop Scramjet.

import { css, type Component, createState } from "dreamland/core";
import {
	CatchEscapedLinksPlugin,
	UrlWatcherPlugin,
} from "@mercuryworkshop/scramjet-utils";
import { versionInfo } from "@mercuryworkshop/scramjet";
import { cachePlugin, controller } from "..";
import { demoSettingsStore } from "../store";
import homepage from "./homepage.html?raw";
import type { Frame } from "@mercuryworkshop/scramjet-controller";

export const browserState = createState({
	url: demoSettingsStore.homeUrl,
	frame: null! as Frame,
});

const shellUrl = new URL(location.href);

function makeShellUrl(destination: string): URL {
	const next = new URL(shellUrl.href);
	next.searchParams.set("goto", destination);
	next.searchParams.set("obermon", "1");
	return next;
}

function createHomepageDataUrl(): string {
	let content = homepage;
	content = content.replaceAll(
		"{{SCRAMJET_VERSION}}",
		String(versionInfo.version)
	);
	content = content.replaceAll("{{SCRAMJET_BUILD}}", String(versionInfo.build));
	content = content.replaceAll(
		"{{SCRAMJET_DATE_PRETTY}}",
		new Date(versionInfo.date).toLocaleString(undefined, {
			dateStyle: "short",
			timeStyle: "short",
		})
	);
	return `data:text/html;base64,${btoa(content)}`;
}

const BrowserView: Component<
	{
		active: boolean;
	},
	{},
	{
		frameel: HTMLIFrameElement;
		pendingUrl?: string;
		urlSyncFrame?: number;
	}
> = function (cx) {
	const scheduleVisibleUrlSync = (destination: string) => {
		if (browserState.url !== destination) browserState.url = destination;
		this.pendingUrl = destination;
		if (this.urlSyncFrame !== undefined) return;

		this.urlSyncFrame = requestAnimationFrame(() => {
			this.urlSyncFrame = undefined;
			const next = this.pendingUrl;
			this.pendingUrl = undefined;
			if (!next || shellUrl.searchParams.get("goto") === next) return;
			shellUrl.searchParams.set("goto", next);
			shellUrl.searchParams.set("obermon", "1");
			history.replaceState(null, "", shellUrl.href);
		});
	};

	cx.mount = async () => {
		await controller.wait();

		const urlWatcher = new UrlWatcherPlugin(scheduleVisibleUrlSync);
		const catchEscapedLinks = new CatchEscapedLinksPlugin((url) =>
			makeShellUrl(url.href)
		);
		const frame = controller.createFrame(this.frameel, {
			plugins: [cachePlugin, urlWatcher, catchEscapedLinks],
		});
		browserState.frame = frame;

		const destination = shellUrl.searchParams.get("goto");
		if (destination) {
			browserState.url = destination;
			frame.go(destination);
			return;
		}

		this.frameel.src = createHomepageDataUrl();
	};

	return (
		<div
			class={use(this.active).map(
				(active) => `browser-view ${active ? "active" : ""}`
			)}
		>
			<iframe this={use(this.frameel)}></iframe>
		</div>
	);
};

BrowserView.style = css`
	:scope {
		flex: 1;
		width: 100%;
		min-width: 0;
		min-height: 0;
		display: none;
		background: #000;
		contain: layout paint style;
	}
	:scope.active {
		display: flex;
	}
	iframe {
		display: block;
		flex: 1;
		width: 100%;
		height: 100%;
		min-width: 0;
		min-height: 0;
		border: 0;
		background: #fff;
		contain: strict;
	}
`;

export default BrowserView;
