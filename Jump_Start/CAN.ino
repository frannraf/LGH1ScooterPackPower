// CAN bus tab.
//
// Blue Pill CAN wiring:
// - PB8 / B8: CAN RX pin on the STM32. Connect to CAN transceiver RXD.
// - PB9 / B9: CAN TX pin on the STM32. Connect to CAN transceiver TXD.
//
// PA11/A11 and PA12/A12 are intentionally left for USB serial.
// Do not connect PB8/PB9 directly to the battery pack CAN wires.
// Battery CANH and CANL must go to the transceiver CANH/CANL pins.

#if !defined(CAN1)
#error "This sketch requires an STM32 part with bxCAN peripheral CAN1."
#endif

#define DEBUG_CAN_REGISTERS 0

enum CanBitrate : uint8_t {
  CAN_50KBPS,
  CAN_100KBPS,
  CAN_125KBPS,
  CAN_250KBPS,
  CAN_500KBPS,
  CAN_1000KBPS,
  CAN_BITRATE_COUNT
};

enum CanPinMap : uint8_t {
  CAN_PINS_PA11_PA12 = 0,
  CAN_PINS_PB8_PB9 = 2,
  CAN_PINS_PD0_PD1 = 3
};

struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t len;
};

struct CanBitTimingConfig {
  uint8_t ts2;
  uint8_t ts1;
  uint16_t brp;
};

static constexpr uint32_t kPackWakeCommandId = 0x630;
static constexpr uint32_t kCanModeTimeoutMs = 1000;
static constexpr uint32_t kCanTxTimeoutUs = 250000;

static constexpr CanBitrate kCanBitrate = CAN_100KBPS;
static constexpr CanPinMap kCanPins = CAN_PINS_PB8_PB9;

static const CanBitTimingConfig kCanBitTiming[CAN_BITRATE_COUNT] = {
  {2, 13, 45},  // 50 kbps
  {2, 15, 20},  // 100 kbps
  {2, 13, 18},  // 125 kbps
  {2, 13, 9},   // 250 kbps
  {2, 15, 4},   // 500 kbps
  {2, 15, 2}    // 1000 kbps
};

static constexpr uint32_t STM32_CAN_TIR_TXRQ = (1UL << 0);
static constexpr uint32_t STM32_CAN_RIR_RTR = (1UL << 1);
static constexpr uint32_t STM32_CAN_RIR_IDE = (1UL << 2);
static constexpr uint32_t STM32_CAN_TIR_RTR = (1UL << 1);
static constexpr uint32_t STM32_CAN_TIR_IDE = (1UL << 2);

static constexpr uint32_t CAN_EXT_ID_MASK = 0x1FFFFFFF;
static constexpr uint32_t CAN_STD_ID_MASK = 0x000007FF;
static constexpr uint32_t CAN_FRAME_RTR = (1UL << 30);

static constexpr uint32_t RCC_APB1ENR_CAN1EN_BIT = (1UL << 25);
static constexpr uint32_t RCC_APB2ENR_AFIOEN_BIT = (1UL << 0);
static constexpr uint32_t RCC_APB2ENR_IOPAEN_BIT = (1UL << 2);
static constexpr uint32_t RCC_APB2ENR_IOPBEN_BIT = (1UL << 3);
static constexpr uint32_t RCC_APB2ENR_IOPDEN_BIT = (1UL << 5);

static void printRegister(const char *label, uint32_t reg);
static bool canInit(CanBitrate bitrate, CanPinMap pinMap);
static void canSetFilter(uint8_t index, uint8_t scale, uint8_t mode, uint8_t fifo, uint32_t bank1, uint32_t bank2);
static bool canSend(const CanMessage &message);
static void canReceive(CanMessage *message);
static uint8_t canMessageAvailable();
static bool findEmptyTxMailbox(uint8_t *mailbox);
static void printCanMessage(const CanMessage &message);
static void printHexId(uint32_t id, uint8_t width);

static bool canBegin()
{
  return canInit(kCanBitrate, kCanPins);
}

static bool canSendPackWakeCommand(bool wakeEnabled)
{
  CanMessage txMessage = {};
  txMessage.id = kPackWakeCommandId;
  txMessage.len = 8;
  txMessage.data[0] = 0x2F;
  txMessage.data[1] = 0x00;
  txMessage.data[2] = 0x22;
  txMessage.data[3] = 0x01;
  txMessage.data[4] = wakeEnabled ? 0x01 : 0x00;
  txMessage.data[5] = 0x00;
  txMessage.data[6] = 0x00;
  txMessage.data[7] = 0x00;

  return canSend(txMessage);
}

static void canPrintReceivedMessages()
{
  CanMessage rxMessage = {};
  while (canMessageAvailable() != 0) {
    canReceive(&rxMessage);
    printCanMessage(rxMessage);
  }
}

static void printRegister(const char *label, uint32_t reg)
{
#if DEBUG_CAN_REGISTERS
  Serial.print(label);
  Serial.print(reg, HEX);
  Serial.println();
#else
  (void)label;
  (void)reg;
#endif
}

static bool canInit(CanBitrate bitrate, CanPinMap pinMap)
{
  const CanBitrate selectedBitrate = bitrate < CAN_BITRATE_COUNT ? bitrate : CAN_100KBPS;
  const CanBitTimingConfig timing = kCanBitTiming[selectedBitrate];

  // Reference manual:
  // https://www.st.com/resource/en/reference_manual/cd00171190.pdf
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN_BIT;
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN_BIT;
  AFIO->MAPR &= 0xFFFF9FFF;  // Reset CAN remap: PA11/A11 RX, PA12/A12 TX.

  if (pinMap == CAN_PINS_PA11_PA12) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN_BIT;
    GPIOA->CRH &= ~(0xFF000UL);
    GPIOA->CRH |= 0xB8FFFUL;
    GPIOA->ODR |= (1UL << 12);  // PA12/A12 pull-up.
  } else if (pinMap == CAN_PINS_PB8_PB9) {
    AFIO->MAPR |= 0x00004000;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN_BIT;
    GPIOB->CRH &= ~(0xFFUL);
    GPIOB->CRH |= 0xB8UL;
    GPIOB->ODR |= (1UL << 8);  // PB8/B8 pull-up.
  } else if (pinMap == CAN_PINS_PD0_PD1) {
    AFIO->MAPR |= 0x00005000;
    RCC->APB2ENR |= RCC_APB2ENR_IOPDEN_BIT;
    GPIOD->CRL &= ~(0xFFUL);
    GPIOD->CRL |= 0xB8UL;
    GPIOD->ODR |= (1UL << 0);  // PD0/D0 pull-up.
  } else {
    Serial.println("Unsupported CAN pin map.");
    return false;
  }

  CAN1->MCR |= 0x1UL;  // Request initialization mode.
  const uint32_t initStartedAt = millis();
  while ((CAN1->MSR & 0x1UL) == 0) {
    if (millis() - initStartedAt >= kCanModeTimeoutMs) {
      return false;
    }
    delay(1);
  }

  // Initialization mode + automatic bus-off management. Automatic
  // retransmission stays enabled because NART remains clear.
  CAN1->MCR = 0x41UL;

  CAN1->BTR &= ~(((0x03UL) << 24) | ((0x07UL) << 20) | ((0x0FUL) << 16) | 0x1FFUL);
  CAN1->BTR |= (((timing.ts2 - 1) & 0x07UL) << 20) |
               (((timing.ts1 - 1) & 0x0FUL) << 16) |
               ((timing.brp - 1) & 0x1FFUL);

  CAN1->FMR |= 0x1UL;       // Filter initialization mode.
  CAN1->FMR &= 0xFFFFC0FF;  // Clear CAN2 start bank.
  CAN1->FMR |= 0x1CUL << 8; // Assign all filters to CAN1.

  // Accept all messages into FIFO 0.
  canSetFilter(0, 1, 0, 0, 0x0UL, 0x0UL);

  CAN1->FMR &= ~(0x1UL);  // Leave filter initialization mode.
  CAN1->MCR &= ~(0x1UL);  // Request normal mode.

  const uint32_t normalModeStartedAt = millis();
  while ((CAN1->MSR & 0x1UL) != 0) {
    if (millis() - normalModeStartedAt >= kCanModeTimeoutMs) {
      return false;
    }
    delay(1);
  }

  printRegister("CAN MCR: 0x", CAN1->MCR);
  printRegister("CAN MSR: 0x", CAN1->MSR);
  printRegister("CAN BTR: 0x", CAN1->BTR);
  return true;
}

static void canSetFilter(uint8_t index, uint8_t scale, uint8_t mode, uint8_t fifo, uint32_t bank1, uint32_t bank2)
{
  if (index > 27) {
    return;
  }

  CAN1->FA1R &= ~(0x1UL << index);

  if (scale == 0) {
    CAN1->FS1R &= ~(0x1UL << index);
  } else {
    CAN1->FS1R |= (0x1UL << index);
  }

  if (mode == 0) {
    CAN1->FM1R &= ~(0x1UL << index);
  } else {
    CAN1->FM1R |= (0x1UL << index);
  }

  if (fifo == 0) {
    CAN1->FFA1R &= ~(0x1UL << index);
  } else {
    CAN1->FFA1R |= (0x1UL << index);
  }

  CAN1->sFilterRegister[index].FR1 = bank1;
  CAN1->sFilterRegister[index].FR2 = bank2;
  CAN1->FA1R |= (0x1UL << index);
}

static bool canSend(const CanMessage &message)
{
  uint8_t mailbox = 0;
  if (!findEmptyTxMailbox(&mailbox)) {
    Serial.println("CAN send fail: no empty transmit mailbox.");
    return false;
  }

  const bool remoteFrame = (message.id & CAN_FRAME_RTR) != 0;
  const uint32_t frameId = message.id & ~CAN_FRAME_RTR;

  uint32_t encodedId = 0;
  if (frameId > CAN_STD_ID_MASK) {
    encodedId = ((frameId & CAN_EXT_ID_MASK) << 3) | STM32_CAN_TIR_IDE;
  } else {
    encodedId = ((frameId & CAN_STD_ID_MASK) << 21);
  }

  if (remoteFrame) {
    encodedId |= STM32_CAN_TIR_RTR;
  }

  CAN1->sTxMailBox[mailbox].TDTR = message.len & 0x0F;
  CAN1->sTxMailBox[mailbox].TDLR = (((uint32_t)message.data[3] << 24) |
                                    ((uint32_t)message.data[2] << 16) |
                                    ((uint32_t)message.data[1] << 8) |
                                    ((uint32_t)message.data[0]));
  CAN1->sTxMailBox[mailbox].TDHR = (((uint32_t)message.data[7] << 24) |
                                    ((uint32_t)message.data[6] << 16) |
                                    ((uint32_t)message.data[5] << 8) |
                                    ((uint32_t)message.data[4]));

  CAN1->sTxMailBox[mailbox].TIR = encodedId | STM32_CAN_TIR_TXRQ;

  const uint32_t sentAt = micros();
  while ((CAN1->sTxMailBox[mailbox].TIR & STM32_CAN_TIR_TXRQ) != 0) {
    if (micros() - sentAt >= kCanTxTimeoutUs) {
      Serial.println("CAN send fail: transmit timeout.");
      Serial.print("ESR: 0x");
      Serial.println(CAN1->ESR, HEX);
      Serial.print("MSR: 0x");
      Serial.println(CAN1->MSR, HEX);
      Serial.print("TSR: 0x");
      Serial.println(CAN1->TSR, HEX);
      return false;
    }
  }

  return true;
}

static bool findEmptyTxMailbox(uint8_t *mailbox)
{
  const uint32_t startedAt = micros();
  while (micros() - startedAt < kCanTxTimeoutUs) {
    const uint32_t tsr = CAN1->TSR;
    if ((tsr & CAN_TSR_TME0) != 0) {
      *mailbox = 0;
      return true;
    }
    if ((tsr & CAN_TSR_TME1) != 0) {
      *mailbox = 1;
      return true;
    }
    if ((tsr & CAN_TSR_TME2) != 0) {
      *mailbox = 2;
      return true;
    }
  }

  return false;
}

static void canReceive(CanMessage *message)
{
  const uint32_t rawId = CAN1->sFIFOMailBox[0].RIR;

  if ((rawId & STM32_CAN_RIR_IDE) == 0) {
    message->id = CAN_STD_ID_MASK & (rawId >> 21);
  } else {
    message->id = CAN_EXT_ID_MASK & (rawId >> 3);
  }

  if ((rawId & STM32_CAN_RIR_RTR) != 0) {
    message->id |= CAN_FRAME_RTR;
  }

  message->len = CAN1->sFIFOMailBox[0].RDTR & 0x0F;

  message->data[0] = 0xFF & CAN1->sFIFOMailBox[0].RDLR;
  message->data[1] = 0xFF & (CAN1->sFIFOMailBox[0].RDLR >> 8);
  message->data[2] = 0xFF & (CAN1->sFIFOMailBox[0].RDLR >> 16);
  message->data[3] = 0xFF & (CAN1->sFIFOMailBox[0].RDLR >> 24);
  message->data[4] = 0xFF & CAN1->sFIFOMailBox[0].RDHR;
  message->data[5] = 0xFF & (CAN1->sFIFOMailBox[0].RDHR >> 8);
  message->data[6] = 0xFF & (CAN1->sFIFOMailBox[0].RDHR >> 16);
  message->data[7] = 0xFF & (CAN1->sFIFOMailBox[0].RDHR >> 24);

  CAN1->RF0R |= 0x20UL;  // Release FIFO 0 output mailbox.
}

static uint8_t canMessageAvailable()
{
  return CAN1->RF0R & 0x3UL;
}

static void printCanMessage(const CanMessage &message)
{
  const bool remoteFrame = (message.id & CAN_FRAME_RTR) != 0;
  const uint32_t frameId = message.id & ~CAN_FRAME_RTR;
  const bool extendedFrame = frameId > CAN_STD_ID_MASK;

  Serial.print(extendedFrame ? "Extended ID: 0x" : "Standard ID: 0x");
  printHexId(frameId, extendedFrame ? 8 : 3);
  Serial.print(remoteFrame ? " RTR" : "    ");
  Serial.print(" DLC: ");
  Serial.print(message.len);
  Serial.print(" Data: ");

  for (uint8_t i = 0; i < message.len && i < sizeof(message.data); i++) {
    Serial.print("0x");
    if (message.data[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(message.data[i], HEX);
    if (i != message.len - 1) {
      Serial.print(" ");
    }
  }

  Serial.println();
}

static void printHexId(uint32_t id, uint8_t width)
{
  for (int8_t nibble = width - 1; nibble >= 0; nibble--) {
    const uint8_t value = (id >> (nibble * 4)) & 0x0F;
    Serial.print(value < 10 ? char('0' + value) : char('A' + value - 10));
  }
}
