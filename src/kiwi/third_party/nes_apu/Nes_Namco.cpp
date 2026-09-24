
// Nes_Snd_Emu 0.1.7. http://www.slack.net/~ant/libs/

#include "Nes_Namco.h"

#include <cstring>

/* Copyright (C) 2003-2005 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include BLARGG_SOURCE_BEGIN

Nes_Namco::Nes_Namco()
{
	output_buffer = NULL;
	enabled = true;
	output( NULL );
	volume( 1.0 );
	reset();
}

Nes_Namco::~Nes_Namco()
{
}

void Nes_Namco::reset( bool clear_ram )
{
	addr_reg = 0;
	last_time = 0;
	enabled = true;
	
	if ( clear_ram )
	{
		for ( int i = 0; i < reg_count; i++ )
			reg [i] = 0;
	}
	
	for ( int i = 0; i < osc_count; i++ )
	{
		Namco_Osc& osc = oscs [i];
		osc.delay = 0;
		osc.last_amp = 0;
		osc.wave_pos = 0;
		osc.output = output_buffer;
	}
}

void Nes_Namco::output( Blip_Buffer* buf )
{
	output_buffer = buf;
	for ( int i = 0; i < osc_count; i++ )
		osc_output( i, enabled ? buf : NULL );
}

std::uint8_t& Nes_Namco::access()
{
	int addr = addr_reg & 0x7f;
	if ( (addr_reg & 0x80) && addr != 0x7f )
		addr_reg = (addr + 1) | 0x80;
	return reg [addr];
}

void Nes_Namco::load_ram( const std::uint8_t* in )
{
	std::memcpy( reg, in, sizeof reg );
}

void Nes_Namco::set_enabled( cpu_time_t time, bool value )
{
	if ( enabled == value )
		return;

	if ( time > last_time )
		run_until( time );
	enabled = value;
	for ( int i = 0; i < osc_count; i++ )
		oscs [i].output = enabled ? output_buffer : NULL;
}

void Nes_Namco::save_snapshot( namco_snapshot_t* out )
{
	std::memcpy( out->regs, reg, sizeof reg );
	out->addr_reg = addr_reg;
	out->enabled = enabled;
	out->unused [0] = 0;
	out->unused [1] = 0;
	out->last_time = static_cast<std::int32_t>( last_time );
	for ( int i = 0; i < osc_count; i++ )
	{
		out->delays [i] = static_cast<std::int32_t>( oscs [i].delay );
		out->last_amps [i] = static_cast<std::int16_t>( oscs [i].last_amp );
		out->wave_positions [i] =
				static_cast<std::int16_t>( oscs [i].wave_pos );
	}
}

void Nes_Namco::load_snapshot( namco_snapshot_t const& in )
{
	reset( false );
	std::memcpy( reg, in.regs, sizeof reg );
	addr_reg = in.addr_reg;
	enabled = in.enabled != 0;
	last_time = in.last_time;
	for ( int i = 0; i < osc_count; i++ )
	{
		oscs [i].delay = in.delays [i];
		oscs [i].last_amp = in.last_amps [i];
		oscs [i].wave_pos = in.wave_positions [i];
		oscs [i].output = enabled ? output_buffer : NULL;
	}
}

void Nes_Namco::end_frame( cpu_time_t time )
{
	if ( time > last_time )
		run_until( time );
	
	last_time -= time;
	assert( last_time >= 0 );
}

#include BLARGG_ENABLE_OPTIMIZER

void Nes_Namco::run_until( cpu_time_t nes_end_time )
{
	int active_oscs = ((reg [0x7f] >> 4) & 7) + 1;
	for ( int i = osc_count - active_oscs; i < osc_count; i++ )
	{
		Namco_Osc& osc = oscs [i];
		Blip_Buffer* output = osc.output;
		if ( !output )
			continue;
		
		Blip_Buffer::resampled_time_t time =
				output->resampled_time( last_time ) + osc.delay;
		Blip_Buffer::resampled_time_t end_time = output->resampled_time( nes_end_time );
		osc.delay = 0;
		if ( time < end_time )
		{
			const std::uint8_t* osc_reg = &reg [i * 8 + 0x40];
			int volume = osc_reg [7] & 15;
			if ( !volume )
				continue;
			
			long freq = (osc_reg [4] & 3) * 0x10000 + osc_reg [2] * 0x100L + osc_reg [0];
			if ( !freq )
				continue;
			Blip_Buffer::resampled_time_t period =
					output->resampled_duration( 983040 ) / freq * active_oscs;
			
			int wave_size = 256 - (osc_reg [4] & 0xfc);
			if ( !wave_size )
				continue;
			
			int last_amp = osc.last_amp;
			int wave_pos = osc.wave_pos;
			
			do
			{
				// read wave sample
				int addr = (wave_pos + osc_reg [6]) & 0xff;
				int sample = reg [addr >> 1];
				wave_pos++;
				if ( addr & 1 )
					sample >>= 4;
				sample = (sample & 15) * volume;
				
				// output impulse if amplitude changed
				int delta = sample - last_amp;
				if ( delta )
				{
					last_amp = sample;
					synth.offset_resampled( time, delta, output );
				}
				
				// next sample
				time += period;
				if ( wave_pos >= wave_size )
					wave_pos = 0;
			}
			while ( time < end_time );
			
			osc.wave_pos = wave_pos;
			osc.last_amp = last_amp;
		}
		osc.delay = time - end_time;
	}
	
	last_time = nes_end_time;
}
