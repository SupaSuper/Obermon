# Security model

## Trust boundaries

1. **Browser process** — trusted Obermon C++ code and profile preferences.
2. **Built-in Scramjet component extension** — trusted, bundled UI and service worker.
3. **Local engine process** — trusted bundled executable; loopback-only and launched with a random session token.
4. **Rewritten destination content** — untrusted web content.

## Required invariants

- Only HTTP and HTTPS destinations are eligible for mediation.
- The internal engine URL is accepted only when its opaque navigation token exists in browser-process memory and matches the destination URL.
- Destination pages cannot call privileged extension APIs.
- The engine cannot listen on non-loopback interfaces.
- Internal URLs are never persisted to history, bookmarks, recently closed tabs, session restore, or copied-address commands.
- Incognito uses a separate engine session and ephemeral state.
- Turning Scramjet off affects new navigations; existing mediated documents remain marked until reloaded directly.
- A failed engine start fails closed and shows an Obermon error page. It never silently falls back to an unrelated remote proxy.

## Explicit non-goals

- TLS interception with a locally installed root certificate.
- Faking Chromium's normal HTTPS padlock for a local origin.
- Allowing arbitrary extensions to request virtual URL substitution.
- Hiding transport information from DevTools or Obermon diagnostics.
