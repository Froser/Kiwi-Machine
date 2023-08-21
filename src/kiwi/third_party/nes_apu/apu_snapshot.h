
// NES APU snapshot support

// Nes_Snd_Emu 0.1.7. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef APU_SNAPSHOT_H
#define APU_SNAPSHOT_H

#include <cstdint>

#include "blargg_common.h"

struct apu_snapshot_t
{
	typedef std::uint8_t byte;
	
	typedef byte env_t [3];
	/*struct env_t {
		byte delay;
		byte env;3
		byte written;
	};*/
	
	byte w40xx [0x14]; // $4000-$4013
	byte w4015; // enables
	byte w4017; // mode
	std::uint16_t delay;
	byte step;
	byte irq_flag;
	
	struct square_t {
		std::uint16_t delay;
		env_t env;
		byte length;
		byte phase;
		byte swp_delay;
		byte swp_reset;
		byte unused [1];
	};
	
	square_t square1;
	square_t square2;
	
	struct triangle_t {
		std::uint16_t delay;
		byte length;
		byte phase;
		byte linear_counter;
		byte linear_mode;
	} triangle;
	
	struct noise_t {
		std::uint16_t delay;
		env_t env;
		byte length;
		std::uint16_t shift_reg;
	} noise;
	
	struct dmc_t {
		std::uint16_t delay;
		std::uint16_t remain;
		std::uint16_t addr;
		byte buf;
		byte bits_remain;
		byte bits;
		byte buf_empty;
		byte silence;
		byte irq_flag;
	} dmc;
	
	enum { tag = 'APUR' };
	void swap();
};
static_assert( sizeof (apu_snapshot_t) == 72 );

#endif

