#pragma once

// Includere SDKDDKVer.h per definire la piattaforma Windows maggiormente disponibile.

// Se si desidera compilare l'applicazione per una piattaforma Windows precedente, includere WinSDKVer.h e
// impostare la macro _WIN32_WINNT sulla piattaforma da supportare prima di includere SDKDDKVer.h.

#define _WIN32_WINNT 0x0502

#define BOOST_WINAPI_VERSION_WIN5

#include <SDKDDKVer.h>
