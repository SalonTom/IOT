import datetime
import sqlite3
import serial
from flask import Flask, redirect, url_for, request, render_template, session, abort
import re

# Pour lancer le server, run la commande suivante :
# flask --app main.py run -h 127.0.0.1 -p 5000


app = Flask(__name__)
app.debug = True

state_list = {
    0: 'Normal',
    1: 'Sécurité'
}

mode_list = {
    0: 'Nuit',
    1: 'Jour'
}

sensor_state = state_list[1]
sensor_mode = mode_list[1]
day_pwd = '8591'
night_pwd = '9482'

conn = sqlite3.connect('projet.db')
bdd = conn.cursor()
bdd.execute('''create table if not exists state (id INTEGER PRIMARY KEY NOT NULL, state INTEGER, date DATETIME DEFAULT CURRENT_TIMESTAMP)''')
bdd.execute('''create table if not exists mode (id INTEGER PRIMARY KEY NOT NULL, mode INTEGER, date DATETIME DEFAULT CURRENT_TIMESTAMP)''')
bdd.execute('''create table if not exists pwd (id INTEGER PRIMARY KEY NOT NULL, night_pwd VARCHAR(4), day_pwd VARCHAR(4), date DATETIME DEFAULT CURRENT_TIMESTAMP)''')
bdd.execute('''create table if not exists logs (id INTEGER PRIMARY KEY NOT NULL, method VARCHAR(255), data VARCHAR(255), date DATETIME DEFAULT CURRENT_TIMESTAMP)''')
conn.commit()
conn.close()


# PORT SERIE
ser = 0


def connect():
    return serial.Serial(
        port='COM10',
        baudrate=9600,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=3
    )


def disconnect(ser):
    ser.close()


# ROUTES
@app.route("/")
def home():
    return render_template('index.html')


@app.route("/state")
async def state():
    currentState = await get_system_data_async(3, 3, 'state', state_list)
    return {
        'data': currentState
    }

@app.route("/mode")
async def mode():
    currentMode = await get_system_data_async(3, 4, 'mode', mode_list)
    return {
        'data': currentMode
    }

@app.route("/logs")
def logs():
    lastLogs = get_logs()
    return {
        'data': lastLogs
    }


def calc_crc(data):
    # Convert input data string to bytes
    data = bytes.fromhex(data.replace('0x', '').replace(' ', ''))

    # Define the CRC16 parameters
    poly = 0xA001
    crc = 0xFFFF

    # Compute the CRC16 checksum
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ poly
            else:
                crc >>= 1

    # Return the CRC16 checksum as an integer
    return crc


def create_modbus_request(fonction, adresse):
    chaine = []

    # Esclave possède l'adresse 1
    chaine.append('0x{0:02x}'.format(1))
    chaine.append('0x{0:02x}'.format(fonction))

    # Pas de numéro d'adresse supérieur à 255
    chaine.append('0x{0:02x}'.format(0))
    chaine.append('0x{0:02x}'.format(adresse))

    # Afin de simplifier les choses, nous ne lisons qu'un registe à la fois
    chaine.append('0x{0:02x}'.format(0))
    chaine.append('0x{0:02x}'.format(1))

    strChaine = ''.join(chaine)

    crc = calc_crc(strChaine)
    print("CRC16: 0x{:04X}".format(crc))

    crc_high_byte = (crc >> 8) & 0xFF
    crc_low_byte = crc & 0xFF

    chaine.append('0x{0:02x}'.format(crc_high_byte))
    chaine.append('0x{0:02x}'.format(crc_low_byte))

    print(chaine)
    return ''.join(chaine)


async def get_system_data_async(function, address, data, enum_data):
    # Connection to the port and retrieving state value
    try:
        chaine = create_modbus_request(function, address)

        # Read data from port
        serial = connect()
        serial.write(bytes(chaine, 'UTF-8'))

        response = serial.readline()
        disconnect(serial)

        # decode serial response
        bytes_response = response.split(b'\x00')[0]
        hex_response_byte = [match.group() for match in re.finditer(r"0x[0-9A-F]{2}", bytes_response.decode("utf-8"))]

        if len(hex_response_byte) != 8:
            print("Erreur lors de la lecture du capteur")
            newValue = 'Erreur lors de la lecture'
        else:
            # Control CRC
            # Decode response data.
            newValueId = int(hex_response_byte[5].replace('0x0', ''))
            newValue = enum_data[newValueId] if newValueId != -1 else 'Inconnu'

            execute_sql_db(f'insert into {data} ({data}) values ({newValueId})')

        # Update logs
        execute_sql_db(f'insert into logs (method, data) values ("READ", "{data}")')

        return newValue
    except:
        # Read last data from database
        newValueId = get_latest_value_from_db(data)
        newValue = enum_data[newValueId] if newValueId != -1 else 'Inconnu'

    return newValue


async def get_state_async():
    # Connection to the port and retrieving state value
    try:
        chaine = create_modbus_request(3, 3)

        # Read data from port
        serial = connect()
        serial.write(bytes(chaine, 'UTF-8'))

        response = serial.readline()
        disconnect(serial)

        # decode serial response
        bytes_response = response.split(b'\x00')[0]
        hex_response_byte = [match.group() for match in re.finditer(r"0x[0-9A-F]{2}", bytes_response.decode("utf-8"))]

        if len(hex_response_byte) != 8:
            print("Erreur lors de la lecture du capteur")
            newValue = 'Erreur lors de la lecture'
        else:
            # Control CRC
            # Decode response data.
            newValueId = int(hex_response_byte[5].replace('0x0', ''))
            newValue = state_list[newValueId] if newValueId != -1 else 'Inconnu'

        # Update logs
        execute_sql_db('insert into logs (method, data) values ("READ", "state")')
        execute_sql_db('insert into state (state) values (1)')

        return newValue
    except:
        # Read last data from database
        newValueId = get_latest_value_from_db('state')
        newValue = state_list[newValueId] if newValueId != -1 else 'Inconnu'

    return newValue


# DATABASE
def get_data_from_db(sqlRequest):
    c = sqlite3.connect('projet.db')
    db = c.cursor()
    db.execute(sqlRequest)
    data = db.fetchall()
    c.commit()
    c.close()

    return data


def execute_sql_db(sqlRequest):
    c = sqlite3.connect('projet.db')
    db = c.cursor()
    db.execute(sqlRequest)
    c.commit()
    c.close()

def get_logs():
    logs = get_data_from_db('select * from logs order by date desc limit 10')
    return logs


def get_latest_value_from_db(table):
    c = sqlite3.connect('projet.db')
    db = c.cursor()
    db.execute('select * from ' + table + ' order by date desc limit 1')
    data = db.fetchall()
    c.commit()
    c.close()
    return (data[0])[1] if (len(data) > 0) else -1


if __name__ == '__main__':
    app.run(debug=True)

# See PyCharm help at https://www.jetbrains.com/help/pycharm/