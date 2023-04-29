function on_load() {
  update_clock();
  document.addEventListener("keydown", keyboard);
}

function update_clock() {
  let date = new Date();
  let h = date.getHours();
  let m = date.getMinutes();
  let s = date.getSeconds();
  document.getElementById('time').innerHTML = `<div>${h}:${ensure_double_digit(m)}:${ensure_double_digit(s)}<div/>`;
  setTimeout(update_clock, 1000);
}

let input = "";

function keyboard(event) {
  let candidate = "";
  let can_add = false;

  if (event.key == "Backspace") {
    candidate = input.substring(0, input.length - 1);
    can_add = true;
  }
  else if (event.key.length == 1) {
    candidate = input + event.key;
  }
  else {
    return;
  }
  let len = candidate.length;

  let hints = document.getElementsByClassName("hint");

  for (let i = 0; i < hints.length; i++) {
    let el = hints[i];
    let text = el.getAttribute('text');

    if (text.length < len) {
      console.log("Not long enough")
      el.innerHTML = text;
    } else if (text === candidate){
      let parent = el.parentElement;
      let link = parent.getElementsByClassName("bookmark");
      let href = link[0].getAttribute("href");
      window.open(href, "_self");
      return;
    } else {
      let it = 0;
      for (; it < len; it++) {
        if(text.charAt(it) !== candidate.charAt(it)) {
          break;
        }
      }
      if (it === 0) {
        el.innerHTML = text;
      } else {
        if (it === len) {
          can_add = true;
        }
        el.innerHTML = `<div class="highlight">${text.substring(0, it)}</div>${text.substring(it)}`
      }
    }
  }
  if(can_add) {
    input = candidate;
  }
  document.getElementById("preview").innerHTML = `${input}`
}

function ensure_double_digit(nr) {
  if (nr < 10) { nr = "0" + nr }
  return nr
}
