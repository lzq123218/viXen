// ATA/ATAPI-4 emulation for the Original Xbox
// (C) Ivan "StrikerX3" Oliveira
//
// This code aims to implement a subset of the ATA/ATAPI-4 specification
// that satisifies the requirements of an IDE interface for the Original Xbox.
//
// Specification:
// http://www.t13.org/documents/UploadedDocuments/project/d1153r18-ATA-ATAPI-4.pdf
//
// References to particular items in the specification are denoted between brackets
// optionally followed by a quote from the specification.
#pragma once

#include <cstdint>

#include <mutex>

#include "vixen/cpu.h"
#include "../basic/irq.h"
#include "../basic/interrupt.h"
#include "ata_device.h"
#include "ata_common.h"

#include "cmds/ata_command.h"
#include "cmds/cmd_identify_device.h"
#include "cmds/cmd_identify_packet_device.h"
#include "cmds/cmd_init_dev_params.h"
#include "cmds/cmd_packet.h"
#include "cmds/cmd_read_dma.h"
#include "cmds/cmd_security_unlock.h"
#include "cmds/cmd_set_features.h"
#include "cmds/cmd_write_dma.h"

namespace vixen {
namespace hw {
namespace ata {

// Map commands to their factories
const std::unordered_map<Command, cmd::IATACommand::Factory, std::hash<uint8_t>> kCmdFactories = {
    { CmdIdentifyDevice, cmd::IdentifyDevice::Factory },
    { CmdIdentifyPacketDevice, cmd::IdentifyPacketDevice::Factory },
    { CmdInitializeDeviceParameters, cmd::InitializeDeviceParameters::Factory },
    { CmdPacket, cmd::Packet::Factory },
    { CmdReadDMA, cmd::ReadDMA::Factory },
    { CmdSecurityUnlock, cmd::SecurityUnlock::Factory },
    { CmdSetFeatures, cmd::SetFeatures::Factory },
    { CmdWriteDMA, cmd::WriteDMA::Factory },
};

// Possible outcomes for DMA transfers
enum DMATransferResult {
    DMATransferOK = 0,
    DMATransferEnd,
    DMATransferError,
};

/*!
 * Represents one of the two ATA channels in a machine (primary or secondary).
 *
 * An ATA channel contains two devices, typically called master and slave.
 */
class ATAChannel {
public:
    ATAChannel(Channel channel, IRQHandler& irqHandler, uint8_t irqNum);
    ~ATAChannel();

    ATADevice& GetDevice(uint8_t deviceIndex) { return *m_devs[deviceIndex]; }

    // ----- Basic I/O --------------------------------------------------------

    bool ReadCommandPort(Register reg, uint32_t *value, uint8_t size);
    bool WriteCommandPort(Register reg, uint32_t value, uint8_t size);

    bool ReadControlPort(uint32_t *value, uint8_t size);
    bool WriteControlPort(uint32_t value, uint8_t size);

    // ----- DMA transfers ----------------------------------------------------

    DMATransferResult ReadDMA(uint8_t *dstBuffer, uint32_t readLen);
    DMATransferResult WriteDMA(uint8_t *srcBuffer, uint32_t writeLen);

    // ----- Interrupts -------------------------------------------------------

    bool AreInterruptsEnabled() { return m_regs.AreInterruptsEnabled(); }
    InterruptTrigger& GetInterruptTrigger() { return m_intrTrigger; }
    void RegisterInterruptHook(InterruptHook *hook) { m_intrHooks.push_back(hook); }

private:
    friend class ATA;

    Channel m_channel;

    // ----- Devices ----------------------------------------------------------

    ATADevice *m_devs[2];

    // ----- Registers --------------------------------------------------------

    // [7.1]: "In this standard, the register contents go to both devices (and
    // their embedded controllers)."
    // This means that the controller can own the set of registers, which is
    // shared by both devices.
    ATARegisters m_regs;

    // ----- State ------------------------------------------------------------

    bool m_interrupt = false;  // [5.2.9] INTRQ (Device Interrupt)
    
    cmd::IATACommand *m_currentCommand;
    std::mutex m_commandMutex;

    // ----- Interrupt handling -----------------------------------------------

    class IntrTrigger : public InterruptTrigger {
    public:
        IntrTrigger(ATAChannel& channel) : m_channel(channel) {}
        void Assert() override { m_channel.SetInterrupt(true); }
        void Negate() override { m_channel.SetInterrupt(false); }
    private:
        ATAChannel& m_channel;
    };

    void SetInterrupt(bool asserted);

    IntrTrigger m_intrTrigger;
    IRQHandler& m_irqHandler;
    uint8_t m_irqNum;

    std::vector<InterruptHook *> m_intrHooks;

    // ----- Command port operations ------------------------------------------

    void ReadData(uint32_t *value, uint8_t size);
    void ReadStatus(uint8_t *value);

    void WriteData(uint32_t value, uint8_t size);
    void WriteCommand(uint8_t value);
};

}
}
}
