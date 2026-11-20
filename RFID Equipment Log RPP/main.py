
import network
import socket
import machine
from machine import UART, Pin
from mfrc522 import MFRC522
import time
import ujson


# ============================================================
# 1. CONFIGURATION
# ============================================================

DATABASE_FILE = "users.json"

MAX_LOGS = 100

# UART1: Pico W <-> STM32
# GP4 = TX
# GP5 = RX

uart = UART(
    1,
    baudrate=115200,
    bits=8,
    parity=None,
    stop=1,
    tx=Pin(4),
    rx=Pin(5)
)

# RFID Reader
rfid = MFRC522()

# Wi-Fi access point
AP_NAME = "Lab-Equipment-Log"
AP_PASSWORD = "labpassword123"


# ============================================================
# 2. GLOBAL DATA
# ============================================================

users = {}

logs = [
    {
        "rfid": "SYSTEM",
        "user": "SYSTEM",
        "item": "INITIALIZED",
        "status": "ONLINE",
        "time": "0"
    }
]

uart_buffer = b""

pending_uid = None
previous_uid = None


# ============================================================
# 3. USER AND ITEM CLASSES
# ============================================================

class Item:

    def __init__(self, item_name, asset_id):

        self.item_name = item_name
        self.asset_id = asset_id


class User:

    def __init__(self, user_name, uid):

        self.user_name = user_name
        self.uid = uid
        self.items = []


# ============================================================
# 4. DATABASE MANAGEMENT
# ============================================================

def load_database():

    global users

    try:

        with open(DATABASE_FILE, "r") as file:

            data = ujson.load(file)

        users = {}

        for uid, user_data in data.items():

            user = User(
                user_data["user_name"],
                user_data["uid"]
            )

            for item_data in user_data["items"]:

                item = Item(
                    item_data["item_name"],
                    item_data["asset_id"]
                )

                user.items.append(item)

            users[uid] = user

        print("Database loaded successfully.")
        print("Registered users:", len(users))

    except OSError:

        print("No database found.")
        users = {}

    except Exception as error:

        print("Database loading error:", error)
        users = {}


def save_database():

    data = {}

    for uid, user in users.items():

        data[uid] = {

            "user_name": user.user_name,

            "uid": user.uid,

            "items": []
        }

        for item in user.items:

            data[uid]["items"].append({

                "item_name": item.item_name,

                "asset_id": item.asset_id

            })

    try:

        with open(DATABASE_FILE, "w") as file:

            ujson.dump(data, file)

        print("Database saved.")

        return True

    except Exception as error:

        print("Database saving error:", error)

        return False


# ============================================================
# 5. USER MANAGEMENT
# ============================================================

def get_user(uid):

    if uid in users:

        return users[uid]

    print("Creating new user:", uid)

    new_user = User(
        user_name="Unknown",
        uid=uid
    )

    users[uid] = new_user

    return new_user


# ============================================================
# 6. ITEM MANAGEMENT
# ============================================================

def checkout_item(user, model_number):

    for item in user.items:

        if item.asset_id == model_number:

            print("ERROR: Item already checked out.")

            return False

    new_item = Item(
        item_name=model_number,
        asset_id=model_number
    )

    user.items.append(new_item)

    print("Item checked out:", model_number)

    return True


def return_item(user, model_number):

    for item in user.items:

        if item.asset_id == model_number:

            user.items.remove(item)

            print("Item returned:", model_number)

            return True

    print("ERROR: Item not found.")

    return False


# ============================================================
# 7. DISPLAY USER INFORMATION
# ============================================================

def print_user(user):

    print("\n========== USER RECORD ==========")

    print("Name:", user.user_name)

    print("UID:", user.uid)

    print("Checked Out Items:")

    if not user.items:

        print("No items currently checked out.")

    else:

        for item in user.items:

            print("-------------------------")

            print("Item:", item.item_name)

            print("Asset ID:", item.asset_id)

    print("=================================\n")


# ============================================================
# 8. WEB DASHBOARD LOGGING
# ============================================================

def add_transaction_log(uid, user, item, status):

    global logs

    transaction = {

        "rfid": uid,

        "user": user.user_name,

        "item": item,

        "status": status,

        "time": get_timestamp()

    }

    logs.insert(0, transaction)

    if len(logs) > MAX_LOGS:

        logs.pop()

    print("[WEB LOG UPDATED]", transaction)


# ============================================================
# 9. UART TRANSMISSION
# ============================================================

def send_uart(message):

    packet = message + "\n"

    uart.write(packet)

    print("UART TX:", message)


def send_ack():

    send_uart("ACK")


def send_nack():

    send_uart("NACK")


# ============================================================
# 10. UART RECEPTION
# ============================================================

def receive_uart():

    global uart_buffer

    messages = []

    if uart.any():

        data = uart.read()

        if data:

            uart_buffer += data

            while b"\n" in uart_buffer:

                line, uart_buffer = uart_buffer.split(
                    b"\n", 1
                )

                try:

                    line = line.decode().strip()

                    if line:

                        messages.append(line)

                except Exception:

                    print("UART decoding error.")

    return messages


# ============================================================
# 11. PROCESS STM32 RESPONSE
# ============================================================

def process_response(uid, response):

    print("\nSTM32 RESPONSE:", response)

    fields = response.split(",")

    if len(fields) != 2:

        print("ERROR: Invalid STM32 packet.")

        send_nack()

        return

    status = fields[0].strip().upper()

    model_number = fields[1].strip()

    if not model_number:

        print("ERROR: Missing model number.")

        send_nack()

        return

    if status not in ("CHECKOUT", "RETURN"):

        print("ERROR: Invalid status.")

        send_nack()

        return

    print("\nTRANSACTION RECEIVED")

    print("Status:", status)

    print("Model:", model_number)

    user = get_user(uid)

    # Process transaction.

    if status == "CHECKOUT":

        success = checkout_item(user, model_number)

    else:

        success = return_item(user, model_number)

    if not success:

        send_nack()

        return

    # Save database before acknowledging.

    if save_database():

        print_user(user)

        # Update web dashboard ONLY after successful save.
        add_transaction_log(
            uid,
            user,
            model_number,
            status
        )

        send_ack()

    else:

        send_nack()


# ============================================================
# 12. EXIT HANDLER
# ============================================================

def process_exit():

    global pending_uid

    print("\nEXIT COMMAND RECEIVED.")

    pending_uid = None

    send_ack()

    print("Waiting for another RFID scan...")


# ============================================================
# 13. RFID SCANNING
# ============================================================

def check_rfid():

    global previous_uid
    global pending_uid

    uid = rfid.read_uid()

    if uid is not None:

        if uid != previous_uid and pending_uid is None:

            previous_uid = uid

            uid_string = "".join(
                "{:02X}".format(byte) for byte in uid
            )

            print("\nRFID CARD DETECTED!")

            print("Card UID:", uid_string)

            pending_uid = uid_string

            send_uart("SCAN," + uid_string)

    else:

        previous_uid = None


# ============================================================
# 14. PROCESS UART MESSAGES
# ============================================================

def check_uart():

    global pending_uid

    messages = receive_uart()

    for message in messages:

        print("UART RX:", message)

        if message == "EXIT":

            process_exit()

        elif pending_uid is not None:

            current_uid = pending_uid

            process_response(
                current_uid,
                message
            )

            pending_uid = None

        else:

            print("Unexpected STM32 message:", message)

            send_nack()


# ============================================================
# 15. WI-FI ACCESS POINT
# ============================================================

def setup_access_point():

    ap = network.WLAN(network.AP_IF)

    ap.active(True)

    ap.config(
        essid=AP_NAME,
        password=AP_PASSWORD
    )

    while not ap.active():

        time.sleep_ms(100)

    ip = ap.ifconfig()[0]

    print("\n================================")

    print("WI-FI ACCESS POINT ACTIVE")

    print("Network:", AP_NAME)

    print("Password:", AP_PASSWORD)

    print("Dashboard: http://" + ip + "/")

    print("================================\n")

    return ip


# ============================================================
# 16. HTML DASHBOARD
# ============================================================

def get_html():

    return """<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>AETHR Lab - Equipment Log</title>

<style>

body {
    font-family: Arial, sans-serif;
    background: #0f172a;
    color: #f8fafc;
    padding: 24px;
    margin: 0;
}

.container {
    max-width: 1000px;
    margin: auto;
}

h1 {
    color: #38bdf8;
    margin-bottom: 4px;
}

.status {
    color: #4ade80;
    font-weight: bold;
    margin-bottom: 24px;
}

.card {
    background: #1e293b;
    border: 1px solid #334155;
    border-radius: 8px;
    overflow-x: auto;
}

table {
    width: 100%;
    border-collapse: collapse;
    text-align: left;
}

th {
    background: #0f172a;
    color: #38bdf8;
    padding: 14px;
}

td {
    padding: 14px;
    border-bottom: 1px solid #334155;
    font-size: 0.95rem;
}

.out {
    color: #fbbf24;
    font-weight: bold;
}

.in {
    color: #4ade80;
    font-weight: bold;
}

</style>

</head>

<body>

<div class="container">

<h1>Dr. Van Neste Lab</h1>

<p class="status">
● Equipment Log System Online
</p>

<div class="card">

<table>

<thead>

<tr>
<th>User Name</th>
<th>User RFID</th>
<th>Equipment Code</th>
<th>Status</th>
<th>Date</th>
</tr>

</thead>

<tbody id="log-table">

<tr>
<td colspan="5" style="text-align:center;">
Waiting for scan transactions...
</td>
</tr>

</tbody>

</table>

</div>

<p style="color:#94a3b8;font-size:0.8rem;">
Dashboard automatically refreshes every second.
</p>

</div>


<script>

function escapeHTML(value) {

    return String(value).replace(/[&<>"']/g, function(c) {

        return {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#39;'
        }[c];

    });

}


async function fetchLogs() {

    try {

        const res = await fetch('/api/logs');

        const data = await res.json();

        let html = '';

        if (data.length === 0) {

            html = `
                <tr>
                    <td colspan="5" style="text-align:center;">
                        No transactions recorded.
                    </td>
                </tr>
            `;

        }

        else {

            data.forEach(item => {

                const cls =
                    (item.status === 'OUT' ||
                     item.status === 'CHECKOUT')
                    ? 'out'
                    : 'in';

                html += `

                <tr>

                    <td>
                        ${escapeHTML(item.user || 'Unknown')}
                    </td>

                    <td>
                        <code>${escapeHTML(item.rfid)}</code>
                    </td>

                    <td>
                        <strong>${escapeHTML(item.item)}</strong>
                    </td>

                    <td class="${cls}">
                        ${escapeHTML(item.status)}
                    </td>

                    <td style="color:#94a3b8;">
                        ${escapeHTML(item.time)}
                    </td>

                </tr>

                `;

            });

        }

        document.getElementById('log-table').innerHTML = html;

    }

    catch (e) {

        console.error("Dashboard update error:", e);

    }

}


// Initial dashboard update.
fetchLogs();

// Refresh every second.
setInterval(fetchLogs, 1000);

</script>

</body>

</html>"""


# ============================================================
# 17. HTTP SERVER
# ============================================================

def setup_server():

    addr = socket.getaddrinfo(
        "0.0.0.0", 80
    )[0][-1]

    server = socket.socket(
        socket.AF_INET,
        socket.SOCK_STREAM
    )

    server.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_REUSEADDR,
        1
    )

    server.bind(addr)

    server.listen(5)

    # Nonblocking server socket.
    server.settimeout(0)

    print("[SERVER READY] HTTP server listening.")

    return server


def handle_http(server):

    try:

        client, address = server.accept()

    except OSError:

        return

    try:

        client.settimeout(0.5)

        request = client.recv(1024)

        if not request:

            client.close()

            return

        request = request.decode("utf-8")

        # Determine requested endpoint.
        request_line = request.split("\r\n")[0]

        if "GET /api/logs " in request_line:

            body = ujson.dumps(logs)

            content_type = "application/json"

        else:

            body = get_html()

            content_type = "text/html"

        response = (
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + content_type + "; charset=utf-8\r\n"
            "Content-Length: " + str(len(body.encode("utf-8"))) + "\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "\r\n"
            + body
        )

        client.sendall(response.encode("utf-8"))

    except Exception as error:

        print("HTTP error:", error)

    finally:

        try:

            client.close()

        except Exception:

            pass

# ============================================================
# 18. REAL-TIME CLOCK
# ============================================================

rtc = machine.RTC()

# Set initial date and time:
# (year, month, day, weekday, hour, minute, second, subseconds)

rtc.datetime((2026, 11, 18, 2, 17, 0, 0, 0))

def get_timestamp():

    current_time = rtc.datetime()

    year = current_time[0]
    month = current_time[1]
    day = current_time[2]

    timestamp = "{:02d}/{:02d}/{:04d}".format(
        month,
        day,
        year
    )

    return timestamp



# ============================================================
# 19. MAIN PROGRAM
# ============================================================

print("\nSTARTING EQUIPMENT TRACKING SYSTEM")

# Initialize RFID reader.
version = rfid.get_version()

print("RC522 Version: 0x{:02X}".format(version))

if version == 0x00 or version == 0xFF:

    print("ERROR: RFID reader not detected.")

else:

    print("RFID reader initialized successfully!")

# Load database.
load_database()

# Start Wi-Fi.
ip_address = setup_access_point()

# Start web server.
server = setup_server()

print("\nSYSTEM READY")

print("Scan an RFID card to begin.")

# Main execution loop.
while True:

    # 1. Check STM32 UART messages.
    check_uart()

    # 2. Check RFID reader.
    check_rfid()

    # 3. Handle incoming web requests.
    handle_http(server)

    # 4. Small delay.
    time.sleep_ms(20)
