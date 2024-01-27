const active = document.getElementById('active');

const m1 = document.getElementById('motor1');
const m2 = document.getElementById('motor2');

const m1In = document.getElementById('m1In');
const m2In = document.getElementById('m2In');

// let screenLock;

// navigator.wakeLock.request('screen')
//     .then(lock => {
//         screenLock = lock;
//     });

let b = 0;
let g = 0;

active.onchange = (event) => {
    console.log('change mode')
    if (active.checked) {
        m1In.disabled = false;
        m2In.disabled = false;
    } else {
        m1In.disabled = true;
        m2In.disabled = true;
    }
}

m1In.oninput = () => {
    b = m1In.value;
    m1.innerText = b;
    console.log(b, g)
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ b, g }))
    }
}

m2In.oninput = () => {
    g = m2In.value;
    m2.innerText = g;
    console.log(b, g)
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ b, g }))
    }
}

const ws = new WebSocket(`wss://${window.location.host}/ws`)

console.log(ws)
ws.onmessage = (e) => {
    const data = JSON.parse(e.data);
    console.log(data)
}

ws.onopen = (e) => {
    console.log('connected to esp')
}