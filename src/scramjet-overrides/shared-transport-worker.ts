// SPDX-License-Identifier: AGPL-3.0-only
// SharedWorker that owns profile-scoped Scramjet transports.

import type {
	ProxyTransport,
	RawHeaders,
	TransferrableResponse,
	WebSocketDataType,
} from "@mercuryworkshop/proxy-transports";
import type { SharedTransportConfig } from "./shared-transport-client";

type TransportConstructor = new (options: { wisp: string }) => ProxyTransport;

type ClientMessage =
	| { type: "init"; config: SharedTransportConfig }
	| {
			type: "request";
			id: number;
			remote: string;
			method: string;
			body: BodyInit | null;
			headers: RawHeaders;
	  }
	| { type: "request-cancel"; id: number }
	| {
			type: "socket-connect";
			id: number;
			url: string;
			protocols: string[];
			requestHeaders: RawHeaders;
	  }
	| { type: "socket-send"; id: number; data: WebSocketDataType }
	| { type: "socket-close-client"; id: number; code: number; reason: string }
	| { type: "client-close" };

type SocketHandle = {
	send: (data: WebSocketDataType) => void;
	close: (code: number, reason: string) => void;
};

type PortState = {
	config: SharedTransportConfig | null;
	requests: Map<number, AbortController>;
	sockets: Map<number, SocketHandle>;
};

const transportCache = new Map<string, Promise<ProxyTransport>>();

function transportKey(config: SharedTransportConfig): string {
	return `${config.partition}\u0000${config.transport}\u0000${config.wisp}`;
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

function getTransport(config: SharedTransportConfig): Promise<ProxyTransport> {
	const key = transportKey(config);
	let existing = transportCache.get(key);
	if (existing) return existing;

	existing = (async () => {
		const Transport = await loadTransportConstructor(config.transport);
		const transport = new Transport({ wisp: config.wisp });
		if (!transport.ready) await transport.init();
		return transport;
	})();
	transportCache.set(key, existing);
	existing.catch(() => transportCache.delete(key));
	return existing;
}

function transferableBody(body: unknown): Transferable[] {
	if (body instanceof ArrayBuffer) return [body];
	if (typeof ReadableStream !== "undefined" && body instanceof ReadableStream) {
		return [body as unknown as Transferable];
	}
	if (ArrayBuffer.isView(body)) return [body.buffer];
	return [];
}

function postResponse(
	port: MessagePort,
	id: number,
	response: TransferrableResponse
): void {
	port.postMessage(
		{ type: "response", id, response },
		transferableBody(response.body)
	);
}

function errorText(error: unknown): string {
	return error instanceof Error ? error.message : String(error);
}

async function handleRequest(
	port: MessagePort,
	state: PortState,
	message: Extract<ClientMessage, { type: "request" }>
): Promise<void> {
	if (!state.config) throw new Error("Shared transport is not initialized.");
	const abortController = new AbortController();
	state.requests.set(message.id, abortController);
	try {
		const transport = await getTransport(state.config);
		if (abortController.signal.aborted) return;
		const response = await transport.request(
			new URL(message.remote),
			message.method,
			message.body,
			message.headers,
			abortController.signal
		);
		if (!abortController.signal.aborted) postResponse(port, message.id, response);
	} catch (error) {
		if (!abortController.signal.aborted) {
			port.postMessage({
				type: "request-error",
				id: message.id,
				error: errorText(error),
			});
		}
	} finally {
		state.requests.delete(message.id);
	}
}

async function handleSocketConnect(
	port: MessagePort,
	state: PortState,
	message: Extract<ClientMessage, { type: "socket-connect" }>
): Promise<void> {
	if (!state.config) throw new Error("Shared transport is not initialized.");
	try {
		const transport = await getTransport(state.config);
		const [send, close] = transport.connect(
			new URL(message.url),
			message.protocols,
			message.requestHeaders,
			(protocol, extensions) =>
				port.postMessage({
					type: "socket-open",
					id: message.id,
					protocol,
					extensions,
				}),
			(data) =>
				port.postMessage(
					{ type: "socket-message", id: message.id, data },
					transferableBody(data)
				),
			(code, reason) => {
				state.sockets.delete(message.id);
				port.postMessage({
					type: "socket-close",
					id: message.id,
					code,
					reason,
				});
			},
			(error) =>
				port.postMessage({
					type: "socket-error",
					id: message.id,
					error,
				})
		);
		state.sockets.set(message.id, { send, close });
	} catch (error) {
		port.postMessage({
			type: "socket-error",
			id: message.id,
			error: errorText(error),
		});
	}
}

function closePortState(state: PortState): void {
	for (const controller of state.requests.values()) controller.abort();
	state.requests.clear();
	for (const socket of state.sockets.values()) {
		try {
			socket.close(1001, "Obermon transport client closed");
		} catch {}
	}
	state.sockets.clear();
}

const workerScope = self as unknown as SharedWorkerGlobalScope;
workerScope.onconnect = (event: MessageEvent) => {
	const port = event.ports[0];
	const state: PortState = {
		config: null,
		requests: new Map(),
		sockets: new Map(),
	};

	port.onmessage = (messageEvent: MessageEvent<ClientMessage>) => {
		const message = messageEvent.data;
		switch (message.type) {
			case "init":
				state.config = message.config;
				void getTransport(message.config)
					.then(() => port.postMessage({ type: "ready" }))
					.catch((error) =>
						port.postMessage({
							type: "init-error",
							error: errorText(error),
						})
					);
				return;
			case "request":
				void handleRequest(port, state, message);
				return;
			case "request-cancel":
				state.requests.get(message.id)?.abort();
				state.requests.delete(message.id);
				return;
			case "socket-connect":
				void handleSocketConnect(port, state, message);
				return;
			case "socket-send":
				state.sockets.get(message.id)?.send(message.data);
				return;
			case "socket-close-client":
				state.sockets.get(message.id)?.close(message.code, message.reason);
				state.sockets.delete(message.id);
				return;
			case "client-close":
				closePortState(state);
				port.close();
				return;
		}
	};
	port.onmessageerror = () => closePortState(state);
	port.start();
};
