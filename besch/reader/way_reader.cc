#include <stdio.h>
#include "../../simdebug.h"
#include "../../utils/simstring.h"

#include "../weg_besch.h"
#include "../intro_dates.h"
#include "../../bauer/wegbauer.h"

#include "way_reader.h"
#include "../obj_node_info.h"
#include "../../dataobj/pakset_info.h"


void way_reader_t::register_obj(obj_besch_t *&data)
{
    weg_besch_t *besch = static_cast<weg_besch_t *>(data);

    wegbauer_t::register_besch(besch);
//    printf("...Weg %s geladen\n", besch->get_name());
	obj_for_xref(get_type(), besch->get_name(), data);

	checksum_t *chk = new checksum_t();
	besch->calc_checksum(chk);
	pakset_info_t::append(besch->get_name(), chk);
}


bool way_reader_t::successfully_loaded() const
{
    return wegbauer_t::alle_wege_geladen();
}


obj_besch_t * way_reader_t::read_node(FILE *fp, obj_node_info_t &node)
{
	ALLOCA(char, besch_buf, node.size);

	weg_besch_t *besch = new weg_besch_t();
	besch->node_info = new obj_besch_t*[node.children];
	// DBG_DEBUG("way_reader_t::read_node()", "node size = %d", node.size);

	// Hajo: Read data
	fread(besch_buf, node.size, 1, fp);
	char * p = besch_buf;

	// Hajo: old versions of PAK files have no version stamp.
	// But we know, the higher most bit was always cleared.
	int version = 0;

	if(node.size == 0) {
		// old node, version 0, compatibility code
		besch->price = 10000;
		besch->maintenance = 800;
		besch->topspeed = 999;
		besch->max_axle_load = 999;
		besch->intro_date = DEFAULT_INTRO_DATE*12;
		besch->obsolete_date = DEFAULT_RETIRE_DATE*12;
		besch->wtyp = road_wt;
		besch->styp = 0;
		besch->draw_as_ding = false;
		besch->number_seasons = 0;
	}
	else {

		const uint16 v = decode_uint16(p);
		version = v & 0x7FFF;

		// Whether the read file is from Simutrans-Experimental
		//@author: jamespetts

		way_constraints_of_way_t way_constraints;
		const bool experimental = version > 0 ? v & EXP_VER : false;
		uint16 experimental_version = 0;
		if(experimental)
		{
			// Experimental version to start at 0 and increment.
			version = version & EXP_VER ? version & 0x3FFF : 0;
			while(version > 0x100)
			{
				version -= 0x100;
				experimental_version ++;
			}
			experimental_version -=1;
		}

		if(version==4  ||  version==5) {
			// Versioned node, version 4+5
			besch->price = decode_uint32(p);
			besch->maintenance = decode_uint32(p);
			besch->topspeed = decode_uint32(p);
			besch->max_axle_load = decode_uint32(p);
			besch->intro_date = decode_uint16(p);
			besch->obsolete_date = decode_uint16(p);
			besch->wtyp = decode_uint8(p);
			besch->styp = decode_uint8(p);
			besch->draw_as_ding = decode_uint8(p);
			besch->number_seasons = decode_sint8(p);
			if(experimental)
			{
				if(experimental_version == 0)
				{
					way_constraints.set_permissive(decode_uint8(p));
					way_constraints.set_prohibitive(decode_uint8(p));
				}
				else
				{
					dbg->fatal( "way_reader_t::read_node()","Incompatible pak file version for Simutrans-E, number %i", experimental_version );
				}
			}

		}
		else if(version==3) {
			// Versioned node, version 3
			besch->price = decode_uint32(p);
			besch->maintenance = decode_uint32(p);
			besch->topspeed = decode_uint32(p);
			besch->max_axle_load = decode_uint32(p);
			besch->intro_date = decode_uint16(p);
			besch->obsolete_date = decode_uint16(p);
			besch->wtyp = decode_uint8(p);
			besch->styp = decode_uint8(p);
			besch->draw_as_ding = decode_uint8(p);
			besch->number_seasons = 0;
		}
		else if(version==2) {
			// Versioned node, version 2
			besch->price = decode_uint32(p);
			besch->maintenance = decode_uint32(p);
			besch->topspeed = decode_uint32(p);
			besch->max_axle_load = decode_uint32(p);
			besch->intro_date = decode_uint16(p);
			besch->obsolete_date = decode_uint16(p);
			besch->wtyp = decode_uint8(p);
			besch->styp = decode_uint8(p);
			besch->draw_as_ding = false;
			besch->number_seasons = 0;
		}
		else if(version == 1) {
			// Versioned node, version 1
			besch->price = decode_uint32(p);
			besch->maintenance = decode_uint32(p);
			besch->topspeed = decode_uint32(p);
			besch->max_axle_load = decode_uint32(p);
			uint32 intro_date= decode_uint32(p);
			besch->intro_date = (intro_date/16)*12 + (intro_date%16);
			besch->wtyp = decode_uint8(p);
			besch->styp = decode_uint8(p);
			besch->obsolete_date = DEFAULT_RETIRE_DATE*12;
			besch->draw_as_ding = false;
			besch->number_seasons = 0;
		}
		else {
			dbg->fatal("way_reader_t::read_node()","Invalid version %d", version);
		}
		besch->set_way_constraints(way_constraints);
	}

	// some internal corrections to pay for previous confusion with two waytypes
	if(besch->wtyp==tram_wt) {
		besch->styp = 7;
		besch->wtyp = track_wt;
	}
	else if(besch->styp==5  &&  besch->wtyp==track_wt) {
		besch->wtyp = monorail_wt;
		besch->styp = 0;
	}
	else if(besch->wtyp==128) {
		besch->wtyp = powerline_wt;
	}
	
	if(version<=2  &&  besch->wtyp==air_wt  &&  besch->topspeed>=250) {
		// runway!
		besch->styp = 1;
	}

	// front images from version 5 on
	besch->front_images = version > 4;

	DBG_DEBUG("way_reader_t::read_node()",
		"version=%d price=%d maintenance=%d topspeed=%d max_axle_load=%d "
		"wtype=%d styp=%d intro_year=%i way_constraints_permissive = %d "
		"way_constraints_prohibitive = %d",
		version,
		besch->price,
		besch->maintenance,
		besch->topspeed,
		besch->max_axle_load,
		besch->wtyp,
		besch->styp,
		besch->intro_date/12,
		besch->get_way_constraints().get_permissive(),
		besch->get_way_constraints().get_prohibitive());

  return besch;
}
