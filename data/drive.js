// keep the screen on
(async () => {
    try {
        const wakeLock = await navigator.wakeLock.request("screen");
    } catch (err) {
        console.log(`${err.name}, ${err.message}`);
    }
})();

const active = document.getElementById('active');

const m1 = document.getElementById('motor1');
const m2 = document.getElementById('motor2');
JoystickController = JoystickController.default

let b = 0;
let g = 0;

active.onchange = (event) => {
    console.log('change mode')
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


const onJoy = (data)=>{
    b = data.x;
    g = data.y;
    console.log(b,g);
    if (ws.readyState === WebSocket.OPEN && active.checked) {
        ws.send(JSON.stringify({ b, g }))
    }
}

const joystick = new JoystickController({ level: 100 }, onJoy);
