// this script runs the sound button on the demo video. the video starts muted, because browsers
// block videos that play sound on their own. the first click starts it again from the beginning
// with sound, and after that the button switches the sound off and on.

const video = document.getElementById("demo-video");
const soundButton = document.getElementById("sound-button");
const soundLabel = soundButton.querySelector(".sound-label");

// this keeps the video still for visitors who have asked their system to reduce motion. the
// button still plays it when clicked.
if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
  video.removeAttribute("autoplay");
  video.pause();
}

let soundTurnedOnBefore = false;

function showSoundState() {
  const soundOn = !video.muted;
  soundButton.classList.toggle("is-on", soundOn);
  soundButton.setAttribute("aria-pressed", String(soundOn));

  if (soundOn) {
    soundLabel.textContent = "Sound on";
  } else {
    soundLabel.textContent = soundTurnedOnBefore ? "Sound off" : "Play with sound";
  }
}

soundButton.addEventListener("click", () => {
  if (video.muted) {
    video.muted = false;

    // the first time, the video restarts so the visitor hears it from the first note.
    if (!soundTurnedOnBefore) {
      video.currentTime = 0;
      soundTurnedOnBefore = true;
    }

    // play() can be refused by the browser, and catch keeps that from showing up as an error.
    video.play().catch(() => {});
  } else {
    video.muted = true;
  }

  showSoundState();
});

soundButton.hidden = false;
showSoundState();
