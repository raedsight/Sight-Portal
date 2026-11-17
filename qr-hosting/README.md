# QR shortlist domain

Firebase Hosting site `sightsolutions-qr` in project `sight-portal-adc29`, served at `qr.sightsolutions.net`.

Only `/s/<id>` is forwarded to the portal's Cloud Run service (`sight-portal`, europe-west2); every other path is a
static page here, so the domain gives no way into the portal. The portal server also refuses non-shortlist paths
that arrive for this host (see `shortlistHostGuard` in `src/serverInsights.ts`).

Deploy from this folder: `npx firebase-tools deploy --only hosting --project sight-portal-adc29`.
This is separate from the AI Studio deployment of the portal itself.
