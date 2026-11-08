# DevScope UI design

Design reference: UI UX Pro Max skill; Minimalism & Swiss Style.
Verified local search: Minimalism & Swiss Style (minimalism-and-swiss-style), desktop professional dashboard fit. Earlier cyberpunk direction superseded by user request for a bright, less oppressive theme.

Native C++ Win32 implementation, no web runtime or remote fonts. Background #F5F7FA, white surfaces, text #1B2B3D, secondary #52677C, accent #146A80, danger #AA2B3D. 16 px Chinese UI font, 32 px heading. 44 px action targets, visible keyboard focus, no animated effects. Sidebar categories; search and sorting over application icons; contextual actions in separate detail rail. Delete uses its own label and red text rather than color alone.

Icon: original geometric D/aperture design, multi-resolution 16–256 px ICO embedded through app.rc. Desktop shortcut uses the executable's embedded icon.

QA: actual native window inspected; startup/close checked. Desktop-only: mobile breakpoints are not applicable. Minimum logical window 1100×760; full per-monitor DPI and screen-reader audit remain outside current validation.

Scan feedback update: no whole-window timer invalidation; child controls clipped during parent paint, partial statistic updates, native marquee progress (Common Controls v6). 2048 is optional after 30 seconds, default response No, once per scan; modeless game window with double-buffered painting, no continuous game animation. Main and game screenshots checked at native size.
