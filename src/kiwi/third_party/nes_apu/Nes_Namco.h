
// Namco 106 sound chip emulator

// Nes_Snd_Emu 0.1.7. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef NES_NAMCO_H
#define NES_NAMCO_H

#include "Nes_Apu.h"

struct namco_snapshot_t;

class Nes_Namco {
public:
	Nes_Namco();
	~Nes_Namco();
	
	// See Nes_Apu.h for reference.
	void volume( double );
	void treble_eq( const blip_eq_t& );
	void output( Blip_Buffer* );
	enum { osc_count = 8 };
	void osc_output( int index, Blip_Buffer* );
	void reset( bool clear_ram = true );
	void end_frame( cpu_time_t );
	void set_enabled( cpu_time_t, bool );
	
	// Read/write data register is at 0x4800
	enum { data_reg_addr = 0x4800 };
	void write_data( cpu_time_t, int );
	int read_data();
	
	// Write-only address register is at 0xF800
	enum { addr_reg_addr = 0xF800 };
	void write_addr( int );
	int peek_data() const;
	const std::uint8_t* ram() const;
	void load_ram( const std::uint8_t* );
	
	void save_snapshot( namco_snapshot_t* out );
	void load_snapshot( namco_snapshot_t const& );
	
private:
	// noncopyable
	Nes_Namco( const Nes_Namco& );
	Nes_Namco& operator = ( const Nes_Namco& );
	
	struct Namco_Osc {
		long delay;
		Blip_Buffer* output;
		short last_amp;
		short wave_pos;
	};
	
	Namco_Osc oscs [osc_count];
	
	cpu_time_t last_time;
	int addr_reg;
	Blip_Buffer* output_buffer;
	bool enabled;
	
	enum { reg_count = 0x80 };
	std::uint8_t reg [reg_count];
	Blip_Synth<blip_good_quality,15> synth;
	
	std::uint8_t& access();
	void run_until( cpu_time_t );
};

struct namco_snapshot_t
{
	std::uint8_t regs [0x80];
	std::uint8_t addr_reg;
	std::uint8_t enabled;
	std::uint8_t unused [2];
	std::int32_t last_time;
	std::int32_t delays [Nes_Namco::osc_count];
	std::int16_t last_amps [Nes_Namco::osc_count];
	std::int16_t wave_positions [Nes_Namco::osc_count];
};
static_assert( sizeof (namco_snapshot_t) == 200 );

inline void Nes_Namco::volume( double v ) { synth.volume( 0.10 / osc_count * v ); }

inline void Nes_Namco::treble_eq( const blip_eq_t& eq ) { synth.treble_eq( eq ); }

inline void Nes_Namco::write_addr( int v ) { addr_reg = v; }

inline int Nes_Namco::read_data() { return access(); }

inline int Nes_Namco::peek_data() const { return reg [addr_reg & 0x7f]; }

inline const std::uint8_t* Nes_Namco::ram() const { return reg; }

inline void Nes_Namco::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = buf;
}

inline void Nes_Namco::write_data( cpu_time_t time, int data )
{
	run_until( time );
	access() = data;
}

#endif
