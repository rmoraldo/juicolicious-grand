// this script points the download button at the right installer for the visitor's computer. it
// asks GitHub for the newest release and finds the Windows (.exe) and macOS (.pkg) files in it,
// so publishing a new release updates the page without editing it. if anything fails, the
// button keeps its original link to the releases page.

const repo = "rmoraldo/juicolicious-grand";
const releasesPage = `https://github.com/${repo}/releases/latest`;

const button = document.getElementById("download-button");
const meta = document.getElementById("download-meta");
const other = document.getElementById("download-other");

// this reads the operating system from the browser. phones and tablets report something else,
// which is handled below since the plugin only runs on computers.
function detectPlatform() {
  const platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "";
  const agent = navigator.userAgent || "";

  if (/iPhone|iPad|iPod|Android/i.test(agent)) return "mobile";
  if (/Win/i.test(platform)) return "windows";
  if (/Mac/i.test(platform)) return "mac";
  return "other";
}

// this turns a file size in bytes into something readable, like 58 MB.
function formatSize(bytes) {
  return `${Math.round(bytes / (1024 * 1024))} MB`;
}

function link(url, text) {
  const a = document.createElement("a");
  a.href = url;
  a.textContent = text;
  return a;
}

async function setUpDownload() {
  const platform = detectPlatform();

  let release;

  try {
    const response = await fetch(`https://api.github.com/repos/${repo}/releases/latest`);
    if (!response.ok) return;
    release = await response.json();
  } catch {
    return;
  }

  const assets = release.assets || [];
  const windows = assets.find((asset) => asset.name.toLowerCase().endsWith(".exe"));
  const mac = assets.find((asset) => asset.name.toLowerCase().endsWith(".pkg"));
  const version = (release.tag_name || "").replace(/^v/, "");

  const builds = {
    windows: { asset: windows, label: "Windows", requirement: "Windows 10 or later, 64 bit" },
    mac: { asset: mac, label: "macOS", requirement: "macOS 11 or later" },
  };

  // on a phone or an unknown system, both installers are listed instead of picking one.
  if (platform === "mobile" || platform === "other") {
    button.textContent = "View all downloads";
    meta.textContent = platform === "mobile"
      ? "Juicolicious Grand runs on Windows and macOS computers."
      : `Version ${version}`;

    other.replaceChildren();
    ["windows", "mac"].forEach((key) => {
      if (!builds[key].asset) return;
      if (other.childNodes.length) other.append(" · ");
      other.append(link(builds[key].asset.browser_download_url, `Download for ${builds[key].label}`));
    });
    return;
  }

  const primary = builds[platform];
  const secondaryKey = platform === "windows" ? "mac" : "windows";
  const secondary = builds[secondaryKey];

  if (primary.asset) {
    button.href = primary.asset.browser_download_url;
    button.textContent = `Download for ${primary.label}`;
    meta.textContent = `Version ${version} · ${formatSize(primary.asset.size)} · ${primary.requirement}`;
  } else {
    button.href = releasesPage;
    button.textContent = "View downloads";
    meta.textContent = `The ${primary.label} version is coming soon.`;
  }

  other.replaceChildren();
  if (secondary.asset) {
    other.append("Also available for ", link(secondary.asset.browser_download_url, secondary.label));
  }
}

setUpDownload();
