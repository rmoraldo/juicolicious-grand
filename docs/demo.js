// this script runs the demo video. it hides the browser's own video controls and uses a big play
// button over the video instead, so it's clear there's something to watch. while the video plays,
// a pause button and a progress bar show along the bottom.

const frame = document.getElementById("video-frame");
const video = document.getElementById("demo-video");
const playButton = document.getElementById("play-button");
const playLabel = document.getElementById("play-label");
const pauseButton = document.getElementById("pause-button");
const progressBar = document.getElementById("progress-bar");

video.removeAttribute("controls");
playButton.hidden = false;
pauseButton.hidden = false;

// this updates which buttons show, and what the play button says, to match the video.
function showState() {
  frame.classList.toggle("is-playing", !video.paused);

  if (video.ended) {
    playLabel.textContent = "Play again";
  } else if (video.currentTime > 0) {
    playLabel.textContent = "Resume";
  } else {
    playLabel.textContent = "Give it a listen";
  }
}

playButton.addEventListener("click", () => {
  if (video.ended) {
    video.currentTime = 0;
  }

  frame.classList.add("has-started");

  // play() can be refused by the browser, and catch keeps that from showing up as an error.
  video.play().catch(() => {});
});

pauseButton.addEventListener("click", () => video.pause());

// clicking the video itself while it plays also pauses it.
video.addEventListener("click", () => {
  if (!video.paused) {
    video.pause();
  }
});

video.addEventListener("play", showState);
video.addEventListener("pause", showState);
video.addEventListener("ended", showState);

// this fills the progress bar as the video plays. requestAnimationFrame runs it once per screen
// refresh, which keeps the bar moving smoothly.
function updateProgress() {
  if (video.duration) {
    progressBar.style.width = `${(video.currentTime / video.duration) * 100}%`;
  }

  requestAnimationFrame(updateProgress);
}

requestAnimationFrame(updateProgress);
showState();
