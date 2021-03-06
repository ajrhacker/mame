// license:BSD-3-Clause
// copyright-holders:David Haywood

// these contain the similar game selections to the games in unk6502_st2xxx.cpp but on updated hardware

/* These use SPI ROMs and unSP2.0 instructions, so will be GeneralPlus branded parts, not SunPlus
   this might just be a GPL16250 with the video features bypassed as the sprite/palette banking is still
   used, but the RAM treated as work buffers
*/

/*

for pcp8728 long jumps are done indirect via a call to RAM

990c 20ec       r4 = 20ec
d9dd            [1d] = r4
990c 0007       r4 = 0007
d9de            [1e] = r4
fe80 28f7       goto 0028f7

the code to handle this is copied in at startup.

Almost all function calls in the game are handled via a call to RAM which copies data inline from SPI for execution
these calls manage their own stack, and copying back the caller function on return etc.

The largest function in RAM at any one time is ~0x600 bytes.

This appears to be incredibly inefficient but the system can't execute directly from SPI ROM, and doesn't have any
RAM outside of the small area internal to the Sunplus SoC

Graphics likewise appear to be loaded pixel by pixel from the SPI to framebuffer every single time there is a draw
call.  Sound is almost certainly handled in the same way.

There is a missing internal ROM that acts as bootstrap and provides some basic functions.  It is at least 0x1000
words in size, with the lowest call being to 0xf000.  It is potentially larger than this.

The internal ROM will also need to provide trampolining for the interrupts, there is a single pointer near the
start of the SPI ROM '02000A: 0041 0002' which points to 20041 (assuming you map the SPI ROM base as word address
0x20000, so that the calls to get code align with ROM addresses)

The function pointed to for the interrupt has the same form of the other functions that get loaded into RAM via
calls to functions in the RAM area.

--------------------------------------------------------

BIOS (internal ROM) calls:

0xf000 - copy dword from SPI using provided pointer

0xf56f - unknown, after some time, done with PC = f56f, only in one place

0xf58f - unknown, soon after startup (only 1 call)

00f7a0 - unknown - 3 calls

0xf931 - unknown, just one call

00fa1d - unknown, just one call

0xfb26 - unknown, after some time (done with pc = fb26 and calls)

0xfb4f - unknown, just one call

0xfbbf - unknown, 3 calls

code currently goes off the rails after some of these unhandled calls (one to f56f?)

--

use 'go 2938' to get to the inline code these load on the fly

the first piece of code copied appears to attempt to checksum the internal BIOS!

*/

#include "emu.h"

#include "cpu/unsp/unsp.h"

#include "emupal.h"
#include "screen.h"
#include "speaker.h"


#define LOG_GPL_UNKNOWN            (1U << 1)
#define LOG_GPL_UNKNOWN_SELECT_SIM (1U << 2)

#define LOG_ALL             (LOG_GPL_UNKNOWN | LOG_GPL_UNKNOWN_SELECT_SIM)
#define VERBOSE             (0)

#include "logmacro.h"


class pcp8718_state : public driver_device
{
public:
	pcp8718_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_mainrom(*this, "maincpu"),
		m_mainram(*this, "mainram"),
		m_palette(*this, "palette"),
		m_screen(*this, "screen"),
		m_spirom(*this, "spi"),
		m_io_p1(*this, "IN0"),
		m_io_p2(*this, "IN1")
	{ }

	void pcp8718(machine_config &config);

	void spi_init();

private:
	virtual void machine_start() override;
	virtual void machine_reset() override;

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	required_device<unsp_20_device> m_maincpu;
	required_region_ptr<uint16_t> m_mainrom;
	required_shared_ptr<uint16_t> m_mainram;

	required_device<palette_device> m_palette;
	required_device<screen_device> m_screen;

	void map(address_map &map);

	uint16_t simulate_f000_r(offs_t offset);

	uint16_t ramcall_2060_logger_r();
	uint16_t ramcall_2189_logger_r();

	uint16_t ramcall_2434_logger_r();

	uint16_t ramcall_2829_logger_r();
	uint16_t ramcall_287a_logger_r();
	uint16_t ramcall_28f7_logger_r();
	uint16_t ramcall_2079_logger_r();


	required_region_ptr<uint8_t> m_spirom;

	uint16_t unk_7abf_r();
	uint16_t unk_7860_r();
	uint16_t unk_780f_r();

	void unk_7860_w(uint16_t data);
	uint16_t m_7860;

	void unk_7868_w(uint16_t data);
	uint16_t unk_7868_r();
	uint16_t m_7868;

	void bankswitch_707e_w(uint16_t data);
	uint16_t bankswitch_707e_r();
	uint16_t m_707e_bank;

	void bankswitch_703a_w(uint16_t data);
	uint16_t bankswitch_703a_r();
	uint16_t m_703a_bank;

	void bankedram_7300_w(offs_t offset, uint16_t data);
	uint16_t bankedram_7300_r(offs_t offset);
	uint16_t m_bankedram_7300[0x400];

	void bankedram_7400_w(offs_t offset, uint16_t data);
	uint16_t bankedram_7400_r(offs_t offset);
	uint16_t m_bankedram_7400[0x800];

	void system_dma_params_channel0_w(offs_t offset, uint16_t data);
	uint16_t system_dma_params_channel0_r(offs_t offset);

	uint16_t m_dmaregs[8];

	void lcd_w(uint16_t data);
	void lcd_command_w(uint16_t data);

	uint16_t spi_misc_control_r();
	uint16_t spi_rx_fifo_r();
	void spi_tx_fifo_w(uint16_t data);

	void spi_control_w(uint16_t data);

	void spi_process_tx_data(uint8_t data);
	uint8_t spi_process_rx();
	uint8_t spi_rx();
	uint8_t spi_rx_fast();

	uint8_t m_rx_fifo[5]; // actually 8 bytes? or 8 half-bytes?

	uint32_t m_spiaddress;

	uint16_t unk_78a1_r();
	uint16_t m_78a1;
	void unk_78d8_w(uint16_t data);

	enum spistate : const int
	{
	   SPI_STATE_READY = 0,
	   SPI_STATE_WAITING_HIGH_ADDR = 1,
	   SPI_STATE_WAITING_MID_ADDR = 2,
	   SPI_STATE_WAITING_LOW_ADDR = 3,
	   // probably not
	   SPI_STATE_WAITING_DUMMY1_ADDR = 4,
	   SPI_STATE_WAITING_DUMMY2_ADDR = 5,
	   SPI_STATE_READING = 6,

	   SPI_STATE_WAITING_HIGH_ADDR_FAST = 8,
	   SPI_STATE_WAITING_MID_ADDR_FAST = 9,
	   SPI_STATE_WAITING_LOW_ADDR_FAST = 10,
	   SPI_STATE_WAITING_LOW_ADDR_FAST_DUMMY = 11,
	   SPI_STATE_READING_FAST = 12

	};

	spistate m_spistate;

	enum lcdstate : const int
	{
		LCD_STATE_READY = 0,
		LCD_STATE_WAITING_FOR_COMMAND = 1,
		LCD_STATE_PROCESSING_COMMAND = 2
	};

	lcdstate m_lcdstate;
	int m_lastlcdcommand;

	uint8_t m_displaybuffer[0x40000];
	int m_lcdaddr;

	uint16_t unk_7870_r();
	required_ioport m_io_p1;
	required_ioport m_io_p2;

	uint16_t m_stored_gamenum;
	int m_addval;

	DECLARE_WRITE_LINE_MEMBER(screen_vblank);
};

uint32_t pcp8718_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	int count = 0;
	for (int y = 0; y < 256; y++)
	{
		uint32_t* dst = &bitmap.pix32(y);

		for (int x = 0; x < 320; x++)
		{
			// 8-bit values get pumped through a 256 word table in intenral ROM and converted to words, so it's probably raw 16-bit RGB data?
			uint16_t dat = m_displaybuffer[(count * 2) + 1] | (m_displaybuffer[(count * 2) + 0] << 8);

			int b = ((dat >> 0) & 0x1f) << 3;
			int g = ((dat >> 5) & 0x3f) << 2;
			int r = ((dat >> 11) & 0x1f) << 3;

			dst[x] = (r << 16) | (g << 8) | (b << 0);
			count++;
		}
	}

	return 0;
}






static INPUT_PORTS_START( pcp8718 )
	PORT_START("IN0")
	PORT_DIPNAME( 0x0001, 0x0001, "P1:0001" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0001, "0001" )
	PORT_DIPNAME( 0x0002, 0x0002, "P1:0002" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0002, "0002" )
	PORT_DIPNAME( 0x0004, 0x0004, "P1:0004" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0004, "0004" )
	PORT_DIPNAME( 0x0008, 0x0008, "P1:0008" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0008, "0008" )
	PORT_DIPNAME( 0x0010, 0x0010, "P1:0010" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0010, "0010" )
	PORT_DIPNAME( 0x0020, 0x0020, "P1:0020" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0020, "0020" )
	PORT_DIPNAME( 0x0040, 0x0040, "P1:0040" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0040, "0040" )
	PORT_DIPNAME( 0x0080, 0x0080, "P1:0080" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0080, "0080" )
	PORT_DIPNAME( 0x0100, 0x0100, "P1:0100" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0100, "0100" )
	PORT_DIPNAME( 0x0200, 0x0200, "P1:0200" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0200, "0200" )
	PORT_DIPNAME( 0x0400, 0x0400, "P1:0400" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0400, "0400" )
	PORT_DIPNAME( 0x0800, 0x0800, "P1:0800" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0800, "0800" )
	PORT_DIPNAME( 0x1000, 0x1000, "P1:1000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x1000, "1000" )
	PORT_DIPNAME( 0x2000, 0x2000, "P1:2000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x2000, "2000" )
	PORT_DIPNAME( 0x4000, 0x4000, "P1:4000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x4000, "4000" )
	PORT_DIPNAME( 0x8000, 0x8000, "P1:8000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x8000, "8000" )

	PORT_START("IN1")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_UNUSED ) // causes lag if state is inverted, investigate
	PORT_DIPNAME( 0x0002, 0x0002, "P2:0002" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0002, "0002" )
	PORT_DIPNAME( 0x0004, 0x0004, "Show Vs in Test Mode" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0004, "0004" )
	PORT_DIPNAME( 0x0008, 0x0008, "P2:0008" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0008, "0008" )
	PORT_DIPNAME( 0x0010, 0x0010, "P2:0010" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x0010, "0010" )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("SOUND")
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_NAME("A")
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_NAME("B")
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("ON/OFF")
	PORT_DIPNAME( 0x1000, 0x1000, "P2:1000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x1000, "1000" )
	PORT_DIPNAME( 0x2000, 0x2000, "P2:2000" )
	PORT_DIPSETTING(      0x0000, "0000" )
	PORT_DIPSETTING(      0x2000, "2000" )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
INPUT_PORTS_END

uint16_t pcp8718_state::unk_7abf_r()
{
	return 0x0001;
}

uint16_t pcp8718_state::unk_7860_r()
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: unk_7860_r (IO port)\n", machine().describe_context());

	uint16_t ret = (m_io_p2->read() & 0xfff7);

	if ((m_7860 & 0x20))
		ret |= 0x08;

	return ret;
}

void pcp8718_state::unk_7860_w(uint16_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: unk_7860_w %04x (IO port)\n", machine().describe_context(), data);
	m_7860 = data;
}


uint16_t pcp8718_state::unk_780f_r()
{
	return 0x0002;
}



uint16_t pcp8718_state::spi_misc_control_r()
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: spi_misc_control_r\n", machine().describe_context());
	return 0x0000;
}


uint16_t pcp8718_state::spi_rx_fifo_r()
{
	if (m_spistate == SPI_STATE_READING_FAST)
		return spi_rx_fast();

	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: spi_rx_fifo_r\n", machine().describe_context());
	return spi_rx();
}

void pcp8718_state::spi_process_tx_data(uint8_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"transmitting %02x\n", data);

	switch (m_spistate)
	{
	case SPI_STATE_READY:
	{
		if (data == 0x03)
		{
			LOGMASKED(LOG_GPL_UNKNOWN,"set to read mode (need address) %02x\n", data);
			m_spistate = SPI_STATE_WAITING_HIGH_ADDR;
		}
		else if (data == 0x0b)
		{
			LOGMASKED(LOG_GPL_UNKNOWN,"set to fast read mode (need address) %02x\n", data);
			m_spistate = SPI_STATE_WAITING_HIGH_ADDR_FAST;
			//machine().debug_break();
		}
		else
		{
			LOGMASKED(LOG_GPL_UNKNOWN,"invalid state request %02x\n", data);
		}
		break;
	}

	case SPI_STATE_WAITING_HIGH_ADDR:
	{
		m_spiaddress = (m_spiaddress & 0xff00ffff) | data << 16;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to high address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_WAITING_MID_ADDR;
		break;
	}

	case SPI_STATE_WAITING_MID_ADDR:
	{
		m_spiaddress = (m_spiaddress & 0xffff00ff) | data << 8;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to mid address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_WAITING_LOW_ADDR;
		break;
	}

	case SPI_STATE_WAITING_LOW_ADDR:
	{
		m_spiaddress = (m_spiaddress & 0xffffff00) | data;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to low address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_READING;
		break;
	}

	case SPI_STATE_READING:
	{
		// writes when in read mode clock in data?
		LOGMASKED(LOG_GPL_UNKNOWN,"write while in read mode (clock data?)\n", data, m_spiaddress);
		break;
	}

	case SPI_STATE_WAITING_DUMMY1_ADDR:
	{
		m_spistate = SPI_STATE_WAITING_DUMMY2_ADDR;
		break;
	}

	case SPI_STATE_WAITING_DUMMY2_ADDR:
	{
	//  m_spistate = SPI_STATE_READY;
		break;
	}


	case SPI_STATE_WAITING_HIGH_ADDR_FAST:
	{
		m_spiaddress = (m_spiaddress & 0xff00ffff) | data << 16;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to high address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_WAITING_MID_ADDR_FAST;
		break;
	}

	case SPI_STATE_WAITING_MID_ADDR_FAST:
	{
		m_spiaddress = (m_spiaddress & 0xffff00ff) | data << 8;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to mid address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_WAITING_LOW_ADDR_FAST;
		break;
	}

	case SPI_STATE_WAITING_LOW_ADDR_FAST:
	{
		m_spiaddress = (m_spiaddress & 0xffffff00) | data;
		LOGMASKED(LOG_GPL_UNKNOWN,"set to low address %02x address is now %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_WAITING_LOW_ADDR_FAST_DUMMY;

		break;
	}

	case SPI_STATE_WAITING_LOW_ADDR_FAST_DUMMY:
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"dummy write %08x\n", data, m_spiaddress);
		m_spistate = SPI_STATE_READING_FAST;
		break;
	}

	case SPI_STATE_READING_FAST:
	{
		// writes when in read mode clock in data?
		LOGMASKED(LOG_GPL_UNKNOWN,"write while in read mode (clock data?)\n", data, m_spiaddress);
		break;
	}

	}
}

uint8_t pcp8718_state::spi_process_rx()
{

	switch (m_spistate)
	{
	case SPI_STATE_READING:
	case SPI_STATE_READING_FAST:
	{
		uint8_t dat = m_spirom[m_spiaddress & 0x3fffff];

		// hack internal BIOS checksum check
		//if (m_spiaddress == ((0x49d13 - 0x20000) * 2)+1)
		//	if (dat == 0x4e)
		//		dat = 0x5e;

		LOGMASKED(LOG_GPL_UNKNOWN,"reading SPI %02x from SPI Address %08x (adjusted word offset %08x)\n", dat, m_spiaddress, (m_spiaddress/2)+0x20000);
		m_spiaddress++;
		return dat;
	}

	default:
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"reading FIFO in unknown state\n");
		return 0x00;
	}
	}

	return 0x00;
}


uint8_t pcp8718_state::spi_rx()
{
	uint8_t ret = m_rx_fifo[0];

	m_rx_fifo[0] = m_rx_fifo[1];
	m_rx_fifo[1] = m_rx_fifo[2];
	m_rx_fifo[2] = m_rx_fifo[3];
	m_rx_fifo[3] = spi_process_rx();

	return ret;
}

uint8_t pcp8718_state::spi_rx_fast()
{
	uint8_t ret = m_rx_fifo[0];

	m_rx_fifo[0] = m_rx_fifo[1];
	m_rx_fifo[1] = m_rx_fifo[2];
	m_rx_fifo[2] = m_rx_fifo[3];
	m_rx_fifo[3] = m_rx_fifo[4];
	m_rx_fifo[4] = spi_process_rx();

	return ret;
}

void pcp8718_state::spi_tx_fifo_w(uint16_t data)
{
	data &= 0x00ff;
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: spi_tx_fifo_w %04x\n", machine().describe_context(), data);

	spi_process_tx_data(data);
}

// this is probably 'port b' but when SPI is enabled some points of this can become SPI control pins
// it's accessed after each large data transfer, probably to reset the SPI into 'ready for command' state?
void pcp8718_state::unk_7868_w(uint16_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: unk_7868_w %04x (Port B + SPI reset?)\n", machine().describe_context(), data);

	if ((m_7868 & 0x0100) != (data & 0x0100))
	{
		if (!(data & 0x0100))
		{
			for (int i = 0; i < 4; i++)
				m_rx_fifo[i] = 0xff;

			m_spistate = SPI_STATE_READY;
		}
	}

	m_7868 = data;
}

uint16_t pcp8718_state::unk_7868_r()
{
	return m_7868;
}

void pcp8718_state::bankswitch_707e_w(uint16_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: bankswitch_707e_w %04x\n", machine().describe_context(), data);
	m_707e_bank = data;
}

uint16_t pcp8718_state::bankswitch_707e_r()
{
	return m_707e_bank;
}


void pcp8718_state::bankswitch_703a_w(uint16_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: bankswitch_703a_w %04x\n", machine().describe_context(), data);
	m_703a_bank = data;
}

uint16_t pcp8718_state::bankswitch_703a_r()
{
	return m_703a_bank;
}

void pcp8718_state::bankedram_7300_w(offs_t offset, uint16_t data)
{
	offset |= (m_703a_bank & 0x000c) << 6;
	m_bankedram_7300[offset] = data;
}

uint16_t pcp8718_state::bankedram_7300_r(offs_t offset)
{
	offset |= (m_703a_bank & 0x000c) << 6;
	return m_bankedram_7300[offset];
}

void pcp8718_state::bankedram_7400_w(offs_t offset, uint16_t data)
{
	if (m_707e_bank & 1)
	{
		m_bankedram_7400[offset + 0x400] = data;
	}
	else 
	{
		m_bankedram_7400[offset] = data;
	}
}

uint16_t pcp8718_state::bankedram_7400_r(offs_t offset)
{
	if (m_707e_bank & 1)
	{
		return m_bankedram_7400[offset + 0x400];
	}
	else 
	{
		return m_bankedram_7400[offset];
	}
}

void pcp8718_state::system_dma_params_channel0_w(offs_t offset, uint16_t data)
{
	m_dmaregs[offset] = data;

	switch (offset)
	{
		case 0:
		{
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Mode)\n", machine().describe_context(), offset, data);

#if 1

			uint16_t mode = m_dmaregs[0];
			uint32_t source = m_dmaregs[1] | (m_dmaregs[4] << 16);
			uint32_t dest = m_dmaregs[2] | (m_dmaregs[5] << 16) ;
			uint32_t length = m_dmaregs[3] | (m_dmaregs[6] << 16);

			if ((mode != 0x0200) && (mode != 0x4009) && (mode != 0x6009))
				fatalerror("unknown dma mode write %04x\n", data);

			if ((mode == 0x4009) || (mode == 0x6009))
			{
				address_space& mem = m_maincpu->space(AS_PROGRAM);

				for (int i = 0; i < length; i++)
				{
					uint16_t dat = mem.read_word(source);

					if (mode & 0x2000)
					{
						// Racing Car and Elevator Action need this logic, the code in gpl16250 should probably be like this
						// but currently gets used in non-increment mode
						mem.write_word(dest, dat & 0xff);
						dest++;
						mem.write_word(dest, dat >> 8);
						dest++;
					}
					else
					{
						mem.write_word(dest, dat);
						dest++;
					}

					source++;
				}
			}
#endif
			break;
		}
		case 1:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Source Low)\n", machine().describe_context(), offset, data);
			break;

		case 2:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Dest Low)\n", machine().describe_context(), offset, data);
			break;

		case 3:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Length Low)\n", machine().describe_context(), offset, data);
			break;

		case 4:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Source High)\n", machine().describe_context(), offset, data);
			break;

		case 5:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Dest High)\n", machine().describe_context(), offset, data);
			break;

		case 6:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA Length High)\n", machine().describe_context(), offset, data);
			break;

		case 7:
			LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_w %01x %04x (DMA unknown)\n", machine().describe_context(), offset, data);
			break;
	}
}

uint16_t pcp8718_state::system_dma_params_channel0_r(offs_t offset)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: system_dma_params_channel0_r %01x\n", machine().describe_context(), offset);
	return m_dmaregs[offset];
}

uint16_t pcp8718_state::unk_7870_r()
{
	logerror("%06x: unk_7870_r (IO port)\n", machine().describe_context());
	return m_io_p2->read();
}

void pcp8718_state::spi_control_w(uint16_t data)
{
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: spi_control_w %04x\n", machine().describe_context(), data);
}

uint16_t pcp8718_state::unk_78a1_r()
{
	// checked in interrupt, code skipped entirely if this isn't set
	return m_78a1;
}

void pcp8718_state::unk_78d8_w(uint16_t data)
{
	// written in IRQ, possible ack
	if (data & 0x8000)
		m_78a1 &= ~0x8000;
}


void pcp8718_state::map(address_map &map)
{
	// there are calls to 01xxx and 02xxx regions
	// (RAM populated by internal ROM?, TODO: check to make sure code copied there isn't from SPI ROM like the GPL16250 bootstrap
	//  does from NAND, it doesn't seem to have a header in the same format at least)
	map(0x000000, 0x006fff).ram().share("mainram");

	// registers at 7xxx are similar to GPL16250, but not identical? (different video system? or just GPL16250 with the video part unused?)

	map(0x00703a, 0x00703a).rw(FUNC(pcp8718_state::bankswitch_703a_r), FUNC(pcp8718_state::bankswitch_703a_w));
	map(0x00707e, 0x00707e).rw(FUNC(pcp8718_state::bankswitch_707e_r), FUNC(pcp8718_state::bankswitch_707e_w));

	map(0x007100, 0x0071ff).ram(); // rowscroll on gpl16250
	map(0x007300, 0x0073ff).rw(FUNC(pcp8718_state::bankedram_7300_r), FUNC(pcp8718_state::bankedram_7300_w)); // palette on gpl16250
	map(0x007400, 0x0077ff).rw(FUNC(pcp8718_state::bankedram_7400_r), FUNC(pcp8718_state::bankedram_7400_w)); // spriteram on gpl16250

	map(0x00780f, 0x00780f).r(FUNC(pcp8718_state::unk_780f_r));


	map(0x007860, 0x007860).rw(FUNC(pcp8718_state::unk_7860_r),FUNC(pcp8718_state::unk_7860_w));
	map(0x007862, 0x007862).nopw();
	map(0x007863, 0x007863).nopw();

	map(0x007868, 0x007868).rw(FUNC(pcp8718_state::unk_7868_r), FUNC(pcp8718_state::unk_7868_w));

	map(0x007870, 0x007870).r(FUNC(pcp8718_state::unk_7870_r)); // I/O

	map(0x0078a1, 0x0078a1).r(FUNC(pcp8718_state::unk_78a1_r));

	map(0x0078d8, 0x0078d8).w(FUNC(pcp8718_state::unk_78d8_w));


	map(0x007940, 0x007940).w(FUNC(pcp8718_state::spi_control_w));
	// 7941 SPI Transmit Status
	map(0x007942, 0x007942).w(FUNC(pcp8718_state::spi_tx_fifo_w));
	// 7943 SPI Receive Status
	map(0x007944, 0x007944).r(FUNC(pcp8718_state::spi_rx_fifo_r));
	map(0x007945, 0x007945).r(FUNC(pcp8718_state::spi_misc_control_r));

	map(0x007a80, 0x007a87).rw(FUNC(pcp8718_state::system_dma_params_channel0_r), FUNC(pcp8718_state::system_dma_params_channel0_w));

	map(0x007abf, 0x007abf).r(FUNC(pcp8718_state::unk_7abf_r));

	// there are calls to 0x0f000 (internal ROM?)
	map(0x00f000, 0x00ffff).rom().region("maincpu", 0x00000);

	// external LCD controller
	map(0x200000, 0x200000).w(FUNC(pcp8718_state::lcd_command_w));
	map(0x20fc00, 0x20fc00).w(FUNC(pcp8718_state::lcd_w));
}

void pcp8718_state::lcd_command_w(uint16_t data)
{
	data &= 0xff;

	switch (m_lcdstate)
	{
	case LCD_STATE_READY:
	case LCD_STATE_PROCESSING_COMMAND:
	{
		if (data == 0x0000)
		{
			m_lcdstate = LCD_STATE_WAITING_FOR_COMMAND;
			m_lastlcdcommand = 0;
		}
		break;
	}

	case LCD_STATE_WAITING_FOR_COMMAND:
	{
		m_lastlcdcommand = data;
		m_lcdstate = LCD_STATE_PROCESSING_COMMAND;
		break;
	}

	}
}

void pcp8718_state::lcd_w(uint16_t data)
{
	data &= 0xff; // definitely looks like 8-bit port as 16-bit values are shifted and rewritten
	LOGMASKED(LOG_GPL_UNKNOWN,"%06x: lcd_w %02x\n", machine().describe_context(), data);

	if ((m_lcdstate == LCD_STATE_PROCESSING_COMMAND) && (m_lastlcdcommand == 0x22))
	{
		m_displaybuffer[m_lcdaddr] = data;
		m_lcdaddr++;

		if (m_lcdaddr >= ((320 * 240) * 2))
			m_lcdaddr = 0;

		//m_lcdaddr &= 0x3ffff;
	}
}

uint16_t pcp8718_state::simulate_f000_r(offs_t offset)
{
	if (!machine().side_effects_disabled())
	{
		uint16_t pc = m_maincpu->state_int(UNSP_PC);
		uint16_t sr = m_maincpu->state_int(UNSP_SR);
		int realpc = (pc | (sr << 16)) & 0x003fffff;

		if ((offset + 0xf000) == (realpc))
		{
			//LOGMASKED(LOG_GPL_UNKNOWN,"simulate_f000_r reading BIOS area (for BIOS call?) %04x\n", offset);
			return m_mainrom[offset];
		}
		else
		{
			//LOGMASKED(LOG_GPL_UNKNOWN,"simulate_f000_r reading BIOS area (for checksum?) %04x\n", offset);
			return m_mainrom[offset];
		}
	}
	return m_mainrom[offset];
}

uint16_t pcp8718_state::ramcall_2060_logger_r()
{
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x2060 in RAM (set SPI to read mode, set address, do dummy FIFO reads)\n");
	}
	return m_mainram[0x2060];
}

uint16_t pcp8718_state::ramcall_2189_logger_r()
{
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x2189 in RAM (unknown)\n");
	}
	return m_mainram[0x2189];
}


uint16_t pcp8718_state::ramcall_2829_logger_r()
{
	// this in turn calls 28f7 but has restore logic too
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x2829 in RAM (load+call function from SPI address %08x)\n", (m_mainram[0x1e] << 16) | m_mainram[0x1d]);
	}
	return m_mainram[0x2829];
}


uint16_t pcp8718_state::ramcall_287a_logger_r()
{
	// this transmits to a device, then reads back the result, needed for menu navigation?!
	// TODO: data should transmit etc. over bits in the I/O ports, this is HLE, although
	// most of this code will end up in a simulation handler for whatever this device is

	if (!machine().side_effects_disabled())
	{
		if (m_maincpu->pc() == 0x287a)
		{
			// 1d = command, 1e = param?
			int command = m_mainram[0x1d] & 0xff;
			int param = m_mainram[0x1e] & 0xff;

			if ((command) == 0x00)  // request result  low
			{
				m_maincpu->set_state_int(UNSP_R1, m_stored_gamenum & 0xff);
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x (request result low)\n", command);
			}
			else if ((command) == 0x01) // request result  high
			{
				m_maincpu->set_state_int(UNSP_R1, (m_stored_gamenum>>8) & 0xff);
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x (request result high)\n", command);
			}
			else if ((command) == 0x02)
			{
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x %02x (set data low)\n", command, param);
				m_stored_gamenum = (m_stored_gamenum & 0xff00) | (m_mainram[0x1e] & 0x00ff);
			}
			else if ((command) == 0x03)
			{
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x %02x (set data high)\n", command, param);
				m_stored_gamenum = (m_stored_gamenum & 0x00ff) | ((m_mainram[0x1e] & 0x00ff) << 8);
			}
			else if ((command) == 0x04)
			{
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x %02x (set add value)\n", command, param);
				// used with down
				if (param == 0x03)
					m_addval = 4;
				else if (param == 0x00)
					m_addval = 0;

				// used if you try to scroll up or left past 0 and the value becomes too large (a negative number)
				// actually writes 0x314 split into 2 commands, so the 2nd write to 0x04 with param then instead 0b/16 sequence of writes instead of 26/0c adds to the high byte?
				if (param == 0x14)
					m_addval = 0x314;

			}
			else if ((command) == 0x05)
			{
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x %02x (set subtract value)\n", command, param);

				// used if you try to scroll down past the and the value becomes too large
				// actually writes 0x313 split into 2 commands, so the 2nd write to 0x05 with param then instead 0b/16 sequence of writes instead of 26/0c subtracts from the high byte?
				if (param == 0x13)
					m_addval = -0x314; // why 314, it writes 313

			}
			else if ((command) == 0x10)
			{
				// this is followed by 0x1b, written if you try to move right off last entry
				m_stored_gamenum = 0x00;
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x (reset value)\n", command);
			}
			else if (command == 0x26)
			{
				// used in direction handlers after writing the first command
				m_stored_gamenum += m_addval;
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x\n", command);
			}
			else if (command == 0x30)
			{
				// used with right
				m_addval = 1;
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x\n", command);
				// 26/0c called after this, then another fixed command value, then 0b/16
				// unlike commands 04/05 there's no parameter byte written here, must be derived from the command? 
			}
			else if (command == 0x37)
			{
				// used with left
				m_addval = -1;
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x\n", command);
				// 26/0c called after this, then another fixed command value, then 0b/16
				// unlike commands 04/05 there's no parameter byte written here, must be derived from the command? 
			}
			else if (command == 0x39)
			{
				// used with up
				m_addval = -4;
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x\n", command);
				// 26/0c called after this, then another fixed command value, then 0b/16
				// unlike commands 04/05 there's no parameter byte written here, must be derived from the command? 
			}
			else
			{
				LOGMASKED(LOG_GPL_UNKNOWN_SELECT_SIM,"call to 0x287a in RAM (transmit / receive) %02x\n", command);
			}

			// hack retf
			return 0x9a90;
		}
	}
	return m_mainram[0x287a];
}

uint16_t pcp8718_state::ramcall_28f7_logger_r()
{
	if (!machine().side_effects_disabled())
	{
		// no  restore logic?
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x28f7 in RAM (load+GO TO function from SPI address %08x)\n", (m_mainram[0x1e] << 16) | m_mainram[0x1d]);
	}
	return m_mainram[0x28f7];
}

uint16_t pcp8718_state::ramcall_2079_logger_r()
{
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x2079 in RAM (maybe drawing related?)\n"); // called in the 'dummy' loop that doesn't actually draw? and other places? as well as after the actual draw command below in the real loop
	}
	return m_mainram[0x2079];
}

uint16_t pcp8718_state::ramcall_2434_logger_r()
{
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_GPL_UNKNOWN,"call to 0x2434 in RAM (drawing related?)\n"); // [1d] as the tile / sprite number, [1e] as xpos, [1f] as ypos, [20] as 0. [21] as ff in some title drawing calls
	}
	return m_mainram[0x2434];
}

void pcp8718_state::machine_start()
{
}

void pcp8718_state::machine_reset()
{
	m_maincpu->space(AS_PROGRAM).install_read_handler(0xf000, 0xffff, read16sm_delegate(*this, FUNC(pcp8718_state::simulate_f000_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x2060, 0x2060, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_2060_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x2079, 0x2079, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_2079_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x2189, 0x2189, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_2189_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x2434, 0x2434, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_2434_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x2829, 0x2829, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_2829_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x287a, 0x287a, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_287a_logger_r)));

	m_maincpu->space(AS_PROGRAM).install_read_handler(0x28f7, 0x28f7, read16smo_delegate(*this, FUNC(pcp8718_state::ramcall_28f7_logger_r)));

	m_spistate = SPI_STATE_READY;
	m_spiaddress = 0;

	for (int i = 0; i < 320*240*2; i++)
		m_displaybuffer[i] = 0x00;

	m_lcdaddr = 0;

	m_lcdstate = LCD_STATE_READY;
	m_lastlcdcommand = 0;

	m_78a1 = 0;
}

WRITE_LINE_MEMBER(pcp8718_state::screen_vblank)
{
	if (state)
	{
		// probably a timer
		m_maincpu->set_input_line(UNSP_IRQ4_LINE, ASSERT_LINE);
		m_78a1 |= 0x8000;
	}
	else
	{
		m_maincpu->set_input_line(UNSP_IRQ4_LINE, CLEAR_LINE);
	}
}

void pcp8718_state::pcp8718(machine_config &config)
{

	UNSP_20(config, m_maincpu, 96000000); // unknown CPU, unsp20 based, 96Mhz is listed as the maximum for most unSP2.0 chips, and appears correct here
	m_maincpu->set_addrmap(AS_PROGRAM, &pcp8718_state::map);

	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_refresh_hz(60);
	m_screen->set_vblank_time(ATTOSECONDS_IN_USEC(10));
	m_screen->set_size(64*8, 32*8);
	m_screen->set_visarea(0*8, 320-1, 0*8, 240-1);
	m_screen->set_screen_update(FUNC(pcp8718_state::screen_update));
	//m_screen->set_palette(m_palette);
	m_screen->screen_vblank().set(FUNC(pcp8718_state::screen_vblank));

	PALETTE(config, m_palette).set_format(palette_device::xBGR_555, 0x8000);
}

// pcp8718 and pcp8728 both contain user data (player name?) and will need to be factory defaulted once they work
// the ROM code is slightly different between them

ROM_START( pcp8718 )
	ROM_REGION( 0x2000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "internal.rom", 0x000000, 0x2000, CRC(ea119561) SHA1(a2680577e20fe1155efc40a5781cf1ec80ccec3a) )

	ROM_REGION( 0x800000, "spi", ROMREGION_ERASEFF )
	//ROM_LOAD16_WORD_SWAP( "8718_en25f32.bin", 0x000000, 0x400000, CRC(cc138db4) SHA1(379af3d94ae840f52c06416d6cf32e25923af5ae) ) // bad dump, some blocks are corrupt
	ROM_LOAD( "eyecare_25q32av1g_ef4016.bin", 0x000000, 0x400000, CRC(58415e10) SHA1(b1adcc03f2ad8d741544204671677740e904ce1a) )
ROM_END

ROM_START( pcp8728 )
	ROM_REGION( 0x2000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "internal.rom", 0x000000, 0x2000, CRC(ea119561) SHA1(a2680577e20fe1155efc40a5781cf1ec80ccec3a) )

	ROM_REGION( 0x800000, "spi", ROMREGION_ERASEFF )
	ROM_LOAD( "pcp 8728 788 in 1.bin", 0x000000, 0x400000, CRC(60115f21) SHA1(e15c39f11e442a76fae3823b6d510178f6166926) )
ROM_END

ROM_START( unkunsp )
	ROM_REGION( 0x2000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "internal.rom", 0x000000, 0x2000, NO_DUMP ) // exact size unknown

	ROM_REGION( 0x800000, "spi", ROMREGION_ERASEFF )
	ROM_LOAD( "fm25q16a.bin", 0x000000, 0x200000, CRC(aeb472ac) SHA1(500c24b725f6d3308ef8cbdf4259f5be556c7c92) )
ROM_END


void pcp8718_state::spi_init()
{
}


CONS( 200?, pcp8718,      0,       0,      pcp8718,   pcp8718, pcp8718_state, spi_init, "PCP", "PCP 8718 - HD 360 Degrees Rocker Palm Eyecare Console - 788 in 1", MACHINE_IS_SKELETON )
CONS( 200?, pcp8728,      0,       0,      pcp8718,   pcp8718, pcp8718_state, spi_init, "PCP", "PCP 8728 - 788 in 1", MACHINE_IS_SKELETON ) // what name was this sold under?

// maybe different hardware, first 0x2000 bytes in ROM is blank, so bootstrap pointers aren't there at least
CONS( 200?, unkunsp,      0,       0,      pcp8718,   pcp8718, pcp8718_state, empty_init, "<unknown>", "unknown unSP-based handheld", MACHINE_IS_SKELETON )
