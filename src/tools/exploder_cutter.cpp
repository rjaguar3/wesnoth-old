/* $Id$ */
/*
   Copyright (C) 2004 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "exploder_cutter.hpp"
#include "../filesystem.hpp"
#include "../sdl_utils.hpp"
#include "SDL_image.h"

cutter::cutter() : verbose_(false)
{

}

const config cutter::load_config(const std::string &filename)
{
	const std::string conf_string = find_configuration(filename);
	const std::string pre_conf_string = preprocess_file(conf_string);

	config res;
	
	try {
		res.read(pre_conf_string);
	} catch(config::error err) {
		throw exploder_failure("Unable to load the configuration for the file " + filename + ": "+ err.message);
	}

	return res;
}


void cutter::load_masks(const config& conf)
{
	const config::child_list& masks = conf.get_children("mask");

	for(config::child_list::const_iterator itor = masks.begin();
		       itor != masks.end(); ++itor) {

		const std::string name = (**itor)["name"];
		const std::string image = get_mask_dir() + "/" + (**itor)["image"];

		if(verbose_) {
			std::cerr << "Adding mask " << name << "\n";
		}

		if(image.empty())
			throw exploder_failure("Missing image for mask " + name);

		int shiftx = 0;
		int shifty = 0;

		if(!((**itor)["shift"]).empty()) {
			std::vector<std::string> shift = config::split((**itor)["shift"]);
			if(shift.size() != 2) 
				throw exploder_failure("Invalid shift " + (**itor)["shift"]);

			shiftx = atoi(shift[0].c_str());
			shifty = atoi(shift[1].c_str());
		}

		if(masks_.find(name) != masks_.end() && masks_[name].filename != image) {
			throw exploder_failure("Mask " + name + 
					" correspond to two different files: " +
					name + " and " +
					masks_.find(name)->second.filename);
		}

		if(masks_.find(name) == masks_.end()) {
			mask& cur_mask = masks_[name];

			cur_mask.filename = image;
			scoped_sdl_surface tmp(IMG_Load(image.c_str()));
			if(tmp == NULL)
				throw exploder_failure("Unable to load mask image " + image);

			cur_mask.image = shared_sdl_surface(make_neutral_surface(tmp));
			cur_mask.shiftx = shiftx;
			cur_mask.shifty = shifty;
		}

		if(masks_[name].image == NULL)
			throw exploder_failure("Unable to load mask image " + image);
	}
}


cutter::surface_map cutter::cut_surface(shared_sdl_surface surf, const config& conf)
{
	surface_map res;

	const config::child_list& config_parts = conf.get_children("part");
	config::child_list::const_iterator itor;

	for(itor = config_parts.begin(); itor != config_parts.end(); ++itor) {
		add_sub_image(surf, res, *itor);
	}

	return res;
}


std::string cutter::find_configuration(const std::string &file)
{
	//finds the file prefix. 
	const std::string fname = file_name(file);
	const std::string::size_type dotpos = fname.rfind('.');
	int underscorepos = fname.find('_');

	//sets "underscore pos" to -1 if there is no underscore.
	if(underscorepos == std::string::npos || underscorepos == file.size()) 
		underscorepos = -1;
	
	std::string basename;
	if(dotpos == std::string::npos || dotpos < underscorepos) {
		basename = file.substr(underscorepos + 1);
	} else {
		basename = file.substr(underscorepos + 1, dotpos - underscorepos - 1);
	}

	return get_exploder_dir() + "/" + basename + ".cfg";
}


void cutter::add_sub_image(const shared_sdl_surface &surf, surface_map &map, const config* config) 
{
	const std::string name = (*config)["name"];
	if(name.empty())
		throw exploder_failure("Un-named sub-image");

	if(masks_.find(name) == masks_.end()) 
		throw exploder_failure("Unable to find mask corresponding to " + name);

	const cutter::mask& mask = masks_[name];

	std::vector<std::string> pos = config::split((*config)["pos"]);
	if(pos.size() != 2) 
		throw exploder_failure("Invalid position " + (*config)["pos"]);

	int x = atoi(pos[0].c_str());
	int y = atoi(pos[1].c_str());

	const SDL_Rect cut = {x + mask.shiftx, y + mask.shifty, mask.image->w, mask.image->h};

	typedef std::pair<std::string, positioned_surface> sme;

	positioned_surface ps;
	ps.image = shared_sdl_surface(::cut_surface(surf, cut));
	if(ps.image == NULL)
		throw exploder_failure("Unable to cut surface!");
	ps.name = name;
	ps.mask = mask;
	ps.x = x + mask.shiftx;
	ps.y = y + mask.shifty;
	map.insert(sme(name, ps));

	if(verbose_) {
		std::cerr << "Extracting sub-image " << name << ", position (" << x << ", " << y << ")\n";
	}
}

void cutter::set_verbose(bool value)
{
	verbose_ = value;
}

