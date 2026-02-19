// --- ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ ---
function toggleMode(checkbox) {
  const printerView = document.getElementById('printerView');
  const debugView = document.getElementById('debugView');
  const labelPrinter = document.getElementById('labelPrinter');
  const labelDebug = document.getElementById('labelDebug');
  const title = document.getElementById('pageTitle');

  if (checkbox.checked) {
    // DEBUG MODE
    printerView.classList.add('hidden');
    debugView.classList.remove('hidden');
    labelPrinter.classList.remove('active');
    labelDebug.classList.add('active');
    title.innerText = "Режим отладки";
    title.style.color = "#ff3b30";
  } else {
    // PRINTER MODE
    printerView.classList.remove('hidden');
    debugView.classList.add('hidden');
    labelPrinter.classList.add('active');
    labelDebug.classList.remove('active');
    title.innerText = "Принтер Брайля";
    title.style.color = "rgba(0,0,0,0.85)";
  }
}

// --- УТИЛИТА СТАТУСА ---
function setDebugStatus(msg, color) {
    const status = document.getElementById('debug-status');
    status.innerHTML = msg;
    status.style.color = color;
}

// --- ЛОГИКА СЕРВОПРИВОДА ---
function sendServo() {
  const angleInput = document.getElementById('servoAngle');
  let angle = parseInt(angleInput.value);

  if (isNaN(angle)) angle = 0;
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  angleInput.value = angle; 

  setDebugStatus("⏳ Поворот серво на " + angle + "°...", "#e67e22");

  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/servo?angle=" + angle, true);
  xhr.onload = function() {
    if(xhr.status == 200) {
      setDebugStatus("✅ Серво: " + angle + "°", "#27ae60");
    } else {
      setDebugStatus("❌ Ошибка HTTP: " + xhr.status, "#c0392b");
    }
  };
  xhr.onerror = function() { setDebugStatus("❌ Ошибка соединения", "#c0392b"); };
  xhr.send();
}

// --- ЛОГИКА ШАГОВИКА ---
let currentStepperDir = 'cw'; // cw = clockwise, ccw = counter-clockwise

function setStepperDir(dir, el) {
    currentStepperDir = dir;
    // Визуальное переключение
    const options = document.querySelectorAll('.segment-option');
    options.forEach(opt => opt.classList.remove('selected'));
    el.classList.add('selected');
}

function sendStepper() {
    const stepsInput = document.getElementById('stepperSteps');
    let steps = parseInt(stepsInput.value);

    if (isNaN(steps) || steps < 1) {
        steps = 1;
        stepsInput.value = 1;
    }

    const dirText = currentStepperDir === 'cw' ? 'по часовой' : 'против часовой';
    setDebugStatus(`⏳ Шаговик: ${steps} шагов ${dirText}...`, "#e67e22");

    var xhr = new XMLHttpRequest();
    // Пример запроса: /stepper?steps=100&dir=cw
    xhr.open("GET", "/stepper?steps=" + steps + "&dir=" + currentStepperDir, true);
    
    xhr.onload = function() {
        if(xhr.status == 200) {
            setDebugStatus(`✅ Шаговик выполнен (${steps})`, "#27ae60");
        } else {
            setDebugStatus("❌ Ошибка HTTP: " + xhr.status, "#c0392b");
        }
    };
    xhr.onerror = function() { setDebugStatus("❌ Ошибка соединения", "#c0392b"); };
    xhr.send();
}

function openSettings() {
    document.getElementById('settingsModal').classList.add('show');
    
    fetch('/get_settings')
        .then(response => response.json())
        .then(data => {
            // Серво
            if(data.a_down !== undefined) document.getElementById('cfg_a_down').value = data.a_down;
            if(data.a_up !== undefined) document.getElementById('cfg_a_up').value = data.a_up;
            
            // Тайминги
            if(data.t_dot !== undefined) document.getElementById('cfg_t_dot').value = data.t_dot;
            if(data.t_ret !== undefined) document.getElementById('cfg_t_ret').value = data.t_ret;
            if(data.t_char !== undefined) document.getElementById('cfg_t_char').value = data.t_char; // NEW
            
            // Механика
            if(data.s_row !== undefined) document.getElementById('cfg_s_row').value = data.s_row;
            if(data.s_new !== undefined) document.getElementById('cfg_s_new').value = data.s_new;
            if(data.c_line !== undefined) document.getElementById('cfg_c_line').value = data.c_line; // NEW
            
            // Направление
            if(data.f_dir !== undefined) document.getElementById('cfg_feed_dir').value = data.f_dir;
        })
        .catch(err => console.log("Ошибка загрузки настроек: " + err));
}

function closeSettings() {
    document.getElementById('settingsModal').classList.remove('show');
}

// Закрытие по клику на темный фон
window.onclick = function(event) {
    const modal = document.getElementById('settingsModal');
    if (event.target == modal) {
        closeSettings();
    }
}

function saveSettings() {
    const params = new URLSearchParams({
        a_down: document.getElementById('cfg_a_down').value,
        a_up: document.getElementById('cfg_a_up').value,
        t_dot: document.getElementById('cfg_t_dot').value,
        t_ret: document.getElementById('cfg_t_ret').value,
        t_char: document.getElementById('cfg_t_char').value,
        s_row: document.getElementById('cfg_s_row').value,
        s_new: document.getElementById('cfg_s_new').value,
        c_line: document.getElementById('cfg_c_line').value,
        f_dir: document.getElementById('cfg_feed_dir').value
    });

    fetch('/save_settings?' + params.toString())
        .then(res => res.text())
        .then(msg => {
            alert("Настройки сохранены! " + msg);
            closeSettings();
        })
        .catch(err => alert("Ошибка сохранения: " + err));
}


// --- ЛОГИКА ПРИНТЕРА ---
const map = {
  'а':[1,0,0,0,0,0], 'б':[1,1,0,0,0,0], 'в':[0,1,0,1,1,1], 'г':[1,1,0,1,1,0],
  'д':[1,0,0,1,1,0], 'е':[1,0,0,0,1,0], 'ё':[1,0,0,0,0,1], 'ж':[0,1,0,1,1,0],
  'з':[1,0,1,0,1,1], 'и':[0,1,0,1,0,0], 'й':[1,1,1,1,0,1], 'к':[1,0,1,0,0,0],
  'л':[1,1,1,0,0,0], 'м':[1,0,1,1,0,0], 'н':[1,0,1,1,1,0], 'о':[1,0,1,0,1,0],
  'п':[1,1,1,1,0,0], 'р':[1,1,1,0,1,0], 'с':[0,1,1,1,0,0], 'т':[0,1,1,1,1,0],
  'у':[1,0,1,0,0,1], 'ф':[1,1,0,1,0,0], 'х':[1,1,0,0,1,0], 'ц':[1,0,0,1,0,0],
  'ч':[1,1,1,1,1,0], 'ш':[1,0,0,0,1,1], 'щ':[1,0,1,1,0,1], 'ъ':[1,1,1,0,1,1],
  'ы':[0,1,1,1,0,1], 'ь':[0,1,1,1,1,1], 'э':[0,1,0,1,0,1], 'ю':[1,1,0,0,1,1],
  'я':[1,1,0,1,0,1], ' ':[0,0,0,0,0,0],
  '1':[1,0,0,0,0,0], '2':[1,1,0,0,0,0], '3':[1,0,0,1,0,0], '4':[1,0,0,1,1,0],
  '5':[1,0,0,0,1,0], '6':[1,1,0,1,0,0], '7':[1,1,0,1,1,0], '8':[1,1,0,0,1,0],
  '9':[0,1,0,1,0,0], '0':[0,1,0,1,1,0],
  '.':[0,1,0,0,1,1], ',':[0,1,0,0,0,0], '!':[0,1,1,0,1,0], '?':[0,1,0,0,0,1], '-':[0,0,1,0,0,1]
};

function renderBraille() {
  const text = document.getElementById("inputText").value.toLowerCase();
  const paper = document.getElementById("paper");
  paper.innerHTML = "";
  
  let line = document.createElement("div");
  line.className = "braille-line";
  paper.appendChild(line);
  
  let count = 0;
  const MAX_CHARS_VISUAL = 13;

  for (let char of text) {
    if (count >= MAX_CHARS_VISUAL || char === '\n') {
      line = document.createElement("div");
      line.className = "braille-line";
      paper.appendChild(line);
      count = 0;
      if(char === '\n') continue;
    }

    let cell = document.createElement("div");
    cell.className = "braille-cell";
    let dots = map[char] || [0,0,0,0,0,0];
    
    for(let i=0; i<6; i++) {
      if(dots[i]) {
        let d = document.createElement("div");
        d.className = "dot p" + (i+1);
        cell.appendChild(d);
      }
    }
    line.appendChild(cell);
    count++;
  }
}

let checkInterval;

function checkStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/status", true);
  xhr.onload = function() {
    if (xhr.status == 200) {
      const statusText = xhr.responseText;
      const statusEl = document.getElementById("status");
      if (statusText === "idle") {
        statusEl.innerHTML = "✅ Печать завершена!";
        statusEl.style.color = "#27ae60";
        clearInterval(checkInterval);
      }
    }
  };
  xhr.send();
}

function sendPrint() {
  const rawText = document.getElementById("inputText").value;
  const text = rawText.toLowerCase(); 
  if(!text) { alert("Введите текст!"); return; }
  
  const status = document.getElementById("status");
  status.innerHTML = "⏳ Отправка данных...";
  status.style.color = "#e67e22";
  
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/print?text=" + encodeURIComponent(text), true);
  xhr.onload = function() {
    if(xhr.status == 200) {
      status.innerHTML = "⚙️ Печать запущена!";
      status.style.color = "#2980b9";
      checkInterval = setInterval(checkStatus, 2500);
    } else {
      status.innerHTML = "❌ Ошибка соединения";
      status.style.color = "#c0392b";
    }
  };
  xhr.onerror = function() {
    status.innerHTML = "❌ ESP не отвечает";
    status.style.color = "#c0392b";
  };
  xhr.send();
}