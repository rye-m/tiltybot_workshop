const active = document.getElementById('active');

const pan = document.getElementById('pan');
const tilt = document.getElementById('tilt');

const panIn = document.getElementById('panIn');
const tiltIn = document.getElementById('tiltIn');

let screenLock;
let b, g = 0;
navigator.wakeLock.request('screen')
    .then(lock => {
        screenLock = lock;
    });


active.onchange = (event) => {
    console.log('change mode')
    if (active.checked) {
        panIn.disabled = true;
        tiltIn.disabled = true;
        if (typeof (DeviceMotionEvent) !== "undefined" && typeof (DeviceMotionEvent.requestPermission) === "function") {
            // Handle iOS 13+ devices.
            DeviceMotionEvent.requestPermission()
                .then((state) => {
                    if (state === 'granted') {
                        window.addEventListener('deviceorientation', handleOrientation, true);
                    } else {
                        console.error('Request to access the orientation was rejected');
                    }
                })
                .catch(console.error);
        } else {
            // Handle regular non iOS 13+ devices.
            console.log('not safari');
            window.addEventListener('deviceorientation', handleOrientation, true);
        }
    } else {
        panIn.disabled = false;
        tiltIn.disabled = false;
    }

}

tiltIn.oninput = () => {
    b = tiltIn.value;
    tilt.innerText = b;
    console.log(b, g)
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ b, g }))
    }
}

panIn.oninput = () => {
    g = panIn.value;
    pan.innerText = g;
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
    // window.addEventListener('deviceorientation', handleOrientation, true);

}

function handleOrientation(event) {
    if (active.checked) {
        b = event.beta.toFixed(3)
        g = event.gamma.toFixed(3)
        console.log(b, g);
        tilt.innerText = b;
        pan.innerText = g;
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ b, g }))
        }
    }
}