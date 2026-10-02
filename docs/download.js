// this script points the two download buttons at the installers in the newest release. it asks
// GitHub for the latest release and finds the Windows (.exe) and macOS (.pkg) files in it, so
// publishing a new release updates the page without editing it. if anything fails, both buttons
// keep their original link to the releases page.

const repo = "rmoraldo/juicolicious-grand";
const releasesPage = `https://github.com/${repo}/releases/latest`;

// each entry matches one button on the page, through its id.
const builds = [
  { id: "windows", label: "Windows", extension: ".exe", requirement: "Windows 10 or later, 64 bit" },
  { id: "mac", label: "macOS", extension: ".pkg", requirement: "macOS 11 or later" },
];

// this turns a file size in bytes into something readable, like 58 MB.
function formatSize(bytes) {
  return `${Math.round(bytes / (1024 * 1024))} MB`;
}

async function setUpDownloads() {
  // phones and tablets can't run the plugin, so they get a short note under the buttons.
  if (/iPhone|iPad|iPod|Android/i.test(navigator.userAgent || "")) {
    document.getElementById("mobile-note").hidden = false;
  }

  let release;

  try {
    const response = await fetch(`https://api.github.com/repos/${repo}/releases/latest`);
    if (!response.ok) return;
    release = await response.json();
  } catch {
    return;
  }

  const assets = release.assets || [];
  const version = (release.tag_name || "").replace(/^v/, "");

  if (version) {
    document.getElementById("download-version").textContent = `Version ${version} · Free download`;
  }

  builds.forEach((build) => {
    const button = document.getElementById(`download-${build.id}`);
    const meta = document.getElementById(`meta-${build.id}`);
    const asset = assets.find((file) => file.name.toLowerCase().endsWith(build.extension));

    if (asset) {
      button.href = asset.browser_download_url;
      meta.textContent = `${formatSize(asset.size)} · ${build.requirement}`;
    } else {
      // this release has no installer for this system yet, so the button is greyed out and
      // links to the releases page instead.
      button.href = releasesPage;
      button.textContent = `${build.label} coming soon`;
      button.classList.add("unavailable");
    }
  });
}

setUpDownloads();
