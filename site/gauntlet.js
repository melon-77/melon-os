// A taste of the installer's gauntlet: five easy questions, shuffled, with a pause before Next and a trip backwards for a wrong answer.
// The real gauntlet is recipes/calamares-melon/modules/gauntlet (200 questions). These are a few of its easy ones.
(function () {
  var bank = [
    ["How many legs does a cat have?", ["4", "2", "6", "8"], "4"],
    ["What is 1 + 1?", ["3", "1", "4", "2"], "2"],
    ["Which C library does melon use?", ["musl", "glibc", "uclibc", "bionic"], "musl"],
    ["Does melon use systemd?", ["no", "yes"], "no"],
    ["Is a ladder a fruit?", ["yes", "no"], "no"],
    ["What colour is sky on a clear day?", ["brown", "grey", "blue", "purple"], "blue"],
    ["What is 5 x 3?", ["17", "15", "14", "16"], "15"],
    ["Which is cold?", ["ice", "toast"], "ice"],
    ["How many minutes are in an hour?", ["60", "100", "24", "30"], "60"],
    ["Is the sun a star?", ["yes", "no"], "yes"],
    ["Do fish live in water?", ["yes", "no"], "yes"],
    ["Which letter comes after A?", ["B", "Z", "Q", "M"], "B"]
  ];
  var WAIT = 3, SHOW = 5, TOTAL = 200, START = 42;   // you join the gauntlet part-way, so a wrong answer has somewhere to send you back to

  var box = document.getElementById("demo");
  if (!box) return;
  var elQ = document.getElementById("d-q"), elOpts = document.getElementById("d-opts"), elMsg = document.getElementById("d-msg"),
      elPos = document.getElementById("d-pos"), elNext = document.getElementById("d-next");
  box.hidden = false;

  function shuffle(a) {
    a = a.slice();
    for (var i = a.length - 1; i > 0; i--) { var j = Math.floor(Math.random() * (i + 1)), t = a[i]; a[i] = a[j]; a[j] = t; }
    return a;
  }

  var deck, n, pos, timer, answered, finished;

  function start() {
    deck = shuffle(bank).slice(0, SHOW); n = 0; pos = START; finished = false; showQuestion();
  }

  function showQuestion() {
    clearInterval(timer);
    var item = deck[n];
    answered = false;
    elQ.textContent = item[0];
    elMsg.textContent = "";
    elPos.textContent = "question " + pos + " of " + TOTAL;
    elNext.disabled = true;
    elNext.textContent = "Next question";
    elOpts.textContent = "";
    shuffle(item[1]).forEach(function (text) {
      var b = document.createElement("button");
      b.type = "button"; b.className = "opt"; b.textContent = text;
      b.addEventListener("click", function () { answer(b, text === item[2]); });
      elOpts.appendChild(b);
    });
  }

  function answer(btn, right) {
    if (answered) return;
    answered = true;
    Array.prototype.forEach.call(elOpts.children, function (b) {
      b.disabled = true;
      if (b.textContent === deck[n][2]) b.classList.add("ok");
    });
    if (!right) {
      btn.classList.add("bad");
      pos -= 10;
      elMsg.textContent = "Wrong. Back 10 questions.";
    } else {
      pos += 1;
      elMsg.textContent = "Correct.";
    }
    var left = WAIT;
    var done = n === SHOW - 1;
    function tick() {
      if (left > 0) { elNext.textContent = (done ? "Finish" : "Next question") + " in " + left; left--; return; }
      clearInterval(timer);
      elNext.disabled = false;
      elNext.textContent = done ? "Finish" : "Next question";
    }
    tick(); timer = setInterval(tick, 1000);
  }

  elNext.addEventListener("click", function () {
    if (finished) { start(); return; }
    if (n === SHOW - 1) {
      finished = true;
      clearInterval(timer);
      elQ.textContent = "That was five. The real gauntlet has " + TOTAL + ".";
      elOpts.textContent = "";
      elPos.textContent = "";
      elMsg.textContent = "You would be on question " + pos + ". The order is shuffled on every run, and every Next button waits.";
      elNext.textContent = "Again";
      return;
    }
    n += 1; showQuestion();
  });

  start();
})();
