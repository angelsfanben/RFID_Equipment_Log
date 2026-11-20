
from machine import Pin, SPI
import time


class MFRC522:

    # MFRC522 register addresses
    CommandReg      = 0x01
    ComIEnReg       = 0x02
    ComIrqReg       = 0x04
    ErrorReg        = 0x06
    FIFODataReg     = 0x09
    FIFOLevelReg    = 0x0A
    ControlReg      = 0x0C
    BitFramingReg   = 0x0D
    ModeReg         = 0x11
    TxControlReg    = 0x14
    TxASKReg        = 0x15
    TModeReg        = 0x2A
    TPrescalerReg   = 0x2B
    TReloadRegH     = 0x2C
    TReloadRegL     = 0x2D
    VersionReg      = 0x37

    # MFRC522 commands
    PCD_Idle        = 0x00
    PCD_Transceive  = 0x0C
    PCD_SoftReset   = 0x0F

    # ISO14443A commands
    PICC_REQA       = 0x26
    PICC_ANTICOLL   = 0x93

    def __init__(self):

        # SPI0: SCK=GP18, MOSI=GP19, MISO=GP16
        self.spi = SPI(
            0,
            baudrate=1_000_000,
            polarity=0,
            phase=0,
            sck=Pin(18),
            mosi=Pin(19),
            miso=Pin(16)
        )

        # Chip select and reset
        self.cs = Pin(17, Pin.OUT, value=1)
        self.rst = Pin(20, Pin.OUT, value=1)

        self.reset()
        self.init()

    # -----------------------------------------
    # LOW-LEVEL REGISTER ACCESS
    # -----------------------------------------

    def write_reg(self, address, value):

        self.cs.value(0)

        self.spi.write(bytes([
            (address << 1) & 0x7E,
            value
        ]))

        self.cs.value(1)

    def read_reg(self, address):

        self.cs.value(0)

        self.spi.write(bytes([
            ((address << 1) & 0x7E) | 0x80,
            0x00
        ]))

        result = self.spi.read(1, 0x00)

        self.cs.value(1)

        return result[0]

    def set_bit_mask(self, address, mask):

        value = self.read_reg(address)
        self.write_reg(address, value | mask)

    def clear_bit_mask(self, address, mask):

        value = self.read_reg(address)
        self.write_reg(address, value & (~mask & 0xFF))

    # -----------------------------------------
    # RESET AND INITIALIZATION
    # -----------------------------------------

    def reset(self):

        self.cs.value(1)

        self.rst.value(0)
        time.sleep_ms(5)

        self.rst.value(1)
        time.sleep_ms(50)

        self.write_reg(self.CommandReg, self.PCD_SoftReset)
        time.sleep_ms(50)

    def init(self):

        # Configure internal timer
        self.write_reg(self.TModeReg, 0x8D)
        self.write_reg(self.TPrescalerReg, 0x3E)
        self.write_reg(self.TReloadRegH, 0x00)
        self.write_reg(self.TReloadRegL, 30)

        # Configure modulation
        self.write_reg(self.TxASKReg, 0x40)
        self.write_reg(self.ModeReg, 0x3D)

        # Enable antenna
        value = self.read_reg(self.TxControlReg)

        if not (value & 0x03):
            self.write_reg(self.TxControlReg, value | 0x03)

        time.sleep_ms(10)

    def get_version(self):

        return self.read_reg(self.VersionReg)

    # -----------------------------------------
    # SPI TRANSCEIVE
    # -----------------------------------------

    def transceive(self, send_data, valid_bits=0):

        # Enable interrupts
        self.write_reg(self.ComIEnReg, 0x77)

        # Clear interrupt flags
        self.write_reg(self.ComIrqReg, 0x7F)

        # Flush FIFO
        self.write_reg(self.FIFOLevelReg, 0x80)

        # Stop previous command
        self.write_reg(self.CommandReg, self.PCD_Idle)

        # Write outgoing data into FIFO
        for byte in send_data:
            self.write_reg(self.FIFODataReg, byte)

        # Start communication
        self.write_reg(self.CommandReg, self.PCD_Transceive)

        # Configure final-byte bit count
        self.write_reg(self.BitFramingReg, valid_bits | 0x80)

        # Poll for response or timeout
        start = time.ticks_ms()

        while True:

            irq = self.read_reg(self.ComIrqReg)

            if irq & 0x30:
                break

            if irq & 0x01:
                return None

            if time.ticks_diff(time.ticks_ms(), start) > 30:
                return None

        # Timer interrupt means no card response
        if irq & 0x01:
            return None

        # Check communication errors
        error = self.read_reg(self.ErrorReg)

        if error & 0x1B:
            return None

        # Read response from FIFO
        length = self.read_reg(self.FIFOLevelReg) & 0x7F

        if length == 0:
            return None

        response = []

        for _ in range(length):
            response.append(self.read_reg(self.FIFODataReg))

        # Clear StartSend
        self.clear_bit_mask(self.BitFramingReg, 0x80)

        return response

    # -----------------------------------------
    # REQUEST CARD
    # -----------------------------------------

    def request(self):

        # REQA is a 7-bit command
        response = self.transceive(
            [self.PICC_REQA],
            valid_bits=7
        )

        if response is None or len(response) != 2:
            return None

        return response

    # -----------------------------------------
    # READ CARD UID
    # -----------------------------------------

    def read_uid(self):

        # First check whether a card is present
        if self.request() is None:
            return None

        # Anti-collision command
        response = self.transceive(
            [self.PICC_ANTICOLL, 0x20]
        )

        if response is None or len(response) != 5:
            return None

        # Verify checksum
        checksum = (
            response[0] ^
            response[1] ^
            response[2] ^
            response[3]
        )

        if checksum != response[4]:
            return None

        # Return four-byte UID
        return response[:4]
