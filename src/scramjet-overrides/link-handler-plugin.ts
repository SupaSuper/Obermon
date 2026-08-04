// SPDX-License-Identifier: AGPL-3.0-only
// Obermon optimized link handling derived from Mercury Workshop Scramjet.

import { ManagedPlugin } from "@mercuryworkshop/scramjet-controller";
import type { Frame } from "@mercuryworkshop/scramjet-controller";
import { EventHandlerPlugin } from "./event-handler-plugin";

export type LinkHandlerPluginOptions = {};

/**
 * Intercepts anchor activation after the page's own listeners have run.
 * A single delegated listener replaces per-anchor listeners and the subtree
 * MutationObserver, avoiding work proportional to total DOM size.
 */
export class LinkHandlerPlugin extends ManagedPlugin {
	constructor(
		private onNewTab: (url: string) => void,
		private options: LinkHandlerPluginOptions = {}
	) {
		super("link-handler", ["event-handler"]);
	}

	install(frame: Frame): void {
		const eventHandler = frame.plugins.find(
			(plugin): plugin is EventHandlerPlugin => plugin.name === "event-handler"
		)!;
		eventHandler.addEventToCapture("click");
		eventHandler.addEventToCapture("auxclick");

		this.tap(
			frame.hooks.init.post,
			(context) => {
				const findAnchor = (event: MouseEvent): HTMLAnchorElement | null => {
					for (const target of event.composedPath()) {
						if (!target || typeof target !== "object") continue;
						const candidate = target as Partial<HTMLAnchorElement>;
						if (candidate.tagName === "A" && typeof candidate.href === "string") {
							return candidate as HTMLAnchorElement;
						}
					}
					return null;
				};

				const activate = (event: MouseEvent, expectedButton: number) => {
					if (event.button !== expectedButton || event.defaultPrevented) return;
					const anchor = findAnchor(event);
					if (!anchor?.href) return;
					event.preventDefault();
					event.stopPropagation();
					event.stopImmediatePropagation();
					this.onNewTab(anchor.href);
				};

				eventHandler.addEventListener(
					context.window.document,
					"click",
					(event: MouseEvent) => activate(event, 0)
				);
				eventHandler.addEventListener(
					context.window.document,
					"auxclick",
					(event: MouseEvent) => activate(event, 1)
				);
			},
			{ after: ["event-handler"] }
		);
	}
}
