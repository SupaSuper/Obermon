# Security model

## Trust boundaries

1. **Browser process** — trusted Obermon C++ code and profile preferences.
2. **Built-in Scramjet component extension** — trusted, bundled UI and service worker.
3. **Local engine process** — trusted bundled executable; loopback-only.
4. **Rewritten destination content** — untrusted web content.

## Required invariants

- Only HTTP and HTTPS destinations are eligible for mediation.
- The browser creates a random in-memory Scramjet session token before it opens an internal controller URL.
- An internal URL becomes a destination virtual URL only when its token belongs to a browser-created session. A forged `127.0.0.1` URL with a chosen `goto` value is rejected.
- The session token survives same-document URL updates within that Scramjet shell so the visible destination can follow in-page and client-side navigation.
- Destination pages cannot call privileged extension APIs.
- The engine cannot listen on non-loopback interfaces.
- Incognito has a separate profile preference; persistent browsing data remains subject to Scramjet's storage behavior and requires end-to-end testing before release.
- Turning Scramjet off affects new navigations and reloads the active eligible tab directly.
- A failed engine launch cancels the mediated navigation rather than silently sending it through an unrelated remote service.

## Explicit non-goals

- TLS interception with a locally installed root certificate.
- Faking Chromium's normal HTTPS security indicator for a local origin.
- Allowing arbitrary extensions to request virtual URL substitution.
- Hiding transport information from DevTools or Obermon diagnostics.
