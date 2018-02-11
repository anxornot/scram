/*
 * Copyright (C) 2014-2018 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file
/// Implementation of functions in Model class.

#include "model.h"

#include "error.h"
#include "ext/find_iterator.h"
#include "ext/multi_index.h"

namespace scram::mef {

Model::Model(std::string name)
    : Element(name.empty() ? kDefaultName : std::move(name)),
      mission_time_(std::make_unique<MissionTime>()) {}

void Model::CheckDuplicateEvent(const Event& event) {
  const std::string& id = event.id();
  if (gates().count(id) || basic_events().count(id) || house_events().count(id))
    SCRAM_THROW(DuplicateElementError())
        << errinfo_element(id, "event")
        << errinfo_container(Element::name(), kTypeString);
}

void Model::Add(HouseEventPtr house_event) {
  CheckDuplicateEvent(*house_event);
  Composite::Add(std::move(house_event));
}

void Model::Add(BasicEventPtr basic_event) {
  CheckDuplicateEvent(*basic_event);
  Composite::Add(std::move(basic_event));
}

void Model::Add(GatePtr gate) {
  CheckDuplicateEvent(*gate);
  Composite::Add(std::move(gate));
}

Formula::ArgEvent Model::GetEvent(const std::string& id) {
  if (auto it = ext::find(basic_events(), id))
    return it->get();
  if (auto it = ext::find(gates(), id))
    return it->get();
  if (auto it = ext::find(house_events(), id))
    return it->get();
  SCRAM_THROW(UndefinedElement("The event " + id + " is not in the model."));
}

}  // namespace scram::mef
