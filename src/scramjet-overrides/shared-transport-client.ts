// SPDX-License-Identifier: AGPL-3.0-only
// Browser-side proxy for Obermon's profile-shared transport worker.

import type {
	ProxyTransport,
	RawHeaders,
	TransferrableResponse,
	WebSocketDataType,
} from "@mercuryworkshop/proxy-transports";

export type SharedTransportConfig = {
	transport: string;
	wisp: string;
	partition: string;
};

type PendingRequest = {
	resolve: (response: TransferrableResponse) => void;
	reject: (error: Error) => void;
	removeAbortListener?: () => void;
};

type SocketCallbacks = {
	onopen: (protocol: string, extensions: string) => void;
	onmessage: (data: WebSocketDataType) => void;
	onclose: (code: number, reason: string) => void;
	onerror: (error: string) => void;
};

type WorkerMessage =
	| { type: "ready" }
	| { type: "init-error"; error: string }
	| { type: "response"; id: number; response: TransferrableResponse }
	| { type: "request-error"; id: number; error: string }
	| { type: "socket-open"; id: number; protocol: string; extensions: string }
	| { type: "socket-message"; id: number; data: WebSocketDataType }
	| { type: "socket-close"; id: number; code: number; reason: string }
	| { type: "socket-error"; id: number; error: string };

function transferableBody(body: unknown): Transferable[] {
	if (body instanceof ArrayBuffer) return [body];
	if (typeof ReadableStream !== "undefined" && body instanceof ReadableStream) {
		return [body as unknown as Transferable];
	}
	if (ArrayBuffer.isView(body)) return [body.buffer];
	return [];
}

function abortError(): DOMException {
	return new DOMException("The operation was aborted.", "AbortError");
}

/**
 * Implements ProxyTransport over a SharedWorker MessagePort. Every tab gets a
 * small client object, while the worker owns one initialized Libcurl/Epoxy
 * transport for each profile partition and configuration.
 */
export class SharedTransportClient implements ProxyTransport {
	ready = false;

	private readonly worker: SharedWorker;
	private readonly port: MessagePort;
	private nextId = 1;
	private initPromise: Promise<void> | null = null;
	private resolveInit: (() => void) | null = null;
	private rejectInit: ((error: Error) => void) | null = null;
	private disposed = false;
	private readonly pendingRequests = new Map<number, PendingRequest>();
	private readonly sockets = new Map<number, SocketCallbacks>();

	constructor(private readonly config: SharedTransportConfig) {
		this.worker = new SharedWorker(
			new URL("./shared-transport-worker.ts", import.meta.url),
			{
				type: "module",
				name: "obermon-profile-transport",
			}
		);
		this.port = this.worker.port;
		this.port.onmessage = (event: MessageEvent<WorkerMessage>) => {
			this.handleMessage(event.data);
		};
		this.port.onmessageerror = () => {
			this.fail(new Error("Shared transport message could not be decoded."));
		};
		this.port.start();
	}

	init = (): Promise<void> => {
		if (this.disposed) return Promise.reject(new Error("Transport is closed."));
		if (this.ready) return Promise.resolve();
		if (this.initPromise) return this.initPromise;

		this.initPromise = new Promise<void>((resolve, reject) => {
			this.resolveInit = resolve;
			this.rejectInit = reject;
		});
		this.port.postMessage({ type: "init", config: this.config });
		return this.initPromise;
	};

	request = async (
		remote: URL,
		method: string,
		body: BodyInit | null,
		headers: RawHeaders,
		signal: AbortSignal | undefined
	): Promise<TransferrableResponse> => {
		await this.init();
		if (signal?.aborted) throw abortError();

		const id = this.nextId++;
		const response = new Promise<TransferrableResponse>((resolve, reject) => {
			const pending: PendingRequest = { resolve, reject };
			if (signal) {
				const onAbort = () => {
					this.port.postMessage({ type: "request-cancel", id });
					this.pendingRequests.delete(id);
				reject(abortError());
				};
				signal.addEventListener("abort", onAbort, { once: true });
				pending.removeAbortListener = () =>
					signal.removeEventListener("abort", onAbort);
			}
			this.pendingRequests.set(id, pending);
		});

		this.port.postMessage(
			{
				type: "request",
				id,
				remote: remote.href,
				method,
				body,
				headers,
			},
			transferableBody(body)
		);
		return response;
	};

	connect = (
		url: URL,
		protocols: string[],
		requestHeaders: RawHeaders,
		onopen: (protocol: string, extensions: string) => void,
		onmessage: (data: WebSocketDataType) => void,
		onclose: (code: number, reason: string) => void,
		onerror: (error: string) => void
	): [
		(data: WebSocketDataType) => void,
		(code: number, reason: string) => void,
	] => {
		const id = this.nextId++;
		this.sockets.set(id, { onopen, onmessage, onclose, onerror });
		void this.init()
			.then(() => {
				this.port.postMessage({
					type: "socket-connect",
					id,
					url: url.href,
					protocols,
					requestHeaders,
				});
			})
			.catch((error) => {
				this.sockets.delete(id);
				onerror(error instanceof Error ? error.message : String(error));
			});

		const send = (data: WebSocketDataType) => {
			if (!this.sockets.has(id)) return;
			this.port.postMessage(
				{ type: "socket-send", id, data },
				transferableBody(data)
			);
		};
		const close = (code: number, reason: string) => {
			if (!this.sockets.has(id)) return;
			this.port.postMessage({ type: "socket-close-client", id, code, reason });
			this.sockets.delete(id);
		};
		return [send, close];
	};

	dispose(): void {
		if (this.disposed) return;
		this.disposed = true;
		this.port.postMessage({ type: "client-close" });
		this.port.close();
		this.fail(new Error("Shared transport client closed."));
	}

	private handleMessage(message: WorkerMessage): void {
		switch (message.type) {
			case "ready":
				this.ready = true;
				this.resolveInit?.();
				this.resolveInit = null;
				this.rejectInit = null;
				return;
			case "init-error":
				this.rejectInit?.(new Error(message.error));
				this.resolveInit = null;
				this.rejectInit = null;
				return;
			case "response": {
				const pending = this.pendingRequests.get(message.id);
				if (!pending) return;
				this.pendingRequests.delete(message.id);
				pending.removeAbortListener?.();
				pending.resolve(message.response);
				return;
			}
			case "request-error": {
				const pending = this.pendingRequests.get(message.id);
				if (!pending) return;
				this.pendingRequests.delete(message.id);
				pending.removeAbortListener?.();
				pending.reject(new Error(message.error));
				return;
			}
			case "socket-open":
				this.sockets.get(message.id)?.onopen(
					message.protocol,
					message.extensions
				);
				return;
			case "socket-message":
				this.sockets.get(message.id)?.onmessage(message.data);
				return;
			case "socket-close": {
				const socket = this.sockets.get(message.id);
				this.sockets.delete(message.id);
				socket?.onclose(message.code, message.reason);
				return;
			}
			case "socket-error":
				this.sockets.get(message.id)?.onerror(message.error);
				return;
		}
	}

	private fail(error: Error): void {
		this.ready = false;
		this.rejectInit?.(error);
		this.resolveInit = null;
		this.rejectInit = null;
		for (const pending of this.pendingRequests.values()) {
			pending.removeAbortListener?.();
			pending.reject(error);
		}
		this.pendingRequests.clear();
		for (const socket of this.sockets.values()) {
			socket.onerror(error.message);
		}
		this.sockets.clear();
	}
}
