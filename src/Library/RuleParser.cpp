//
// Copyright (C) 2015 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#include <build-config.h>

#include "RuleParser.hpp"
#include "RuleParser/Grammar.hpp"
#include "RuleParser/Actions.hpp"

#include "Typedefs.hpp"
#include "RulePrivate.hpp"
#include "USB.hpp"
#include "Common/Utility.hpp"
#include "LoggerPrivate.hpp"

#include <cstddef>
#include <stdexcept>
#include <stdlib.h>

namespace usbguard
{
  Rule parseRuleFromString(const String& rule_spec, const String& file, size_t line)
  {
    logger->debug("Trying to parse rule: \"{}\"", rule_spec);

    try {
      Rule rule;
      pegtl::parse_string<RuleParser::rule_grammar, RuleParser::rule_parser_actions>(rule_spec, file, rule);
      return rule;
    }
    catch(const pegtl::parse_error& ex) {
      RuleParserError error(rule_spec);
      
      error.setHint(ex.what());
      error.setOffset(ex.positions[0].byte_in_line);

      if (!file.empty()) {
        error.setFileInfo(file, line);
      }

      throw error;
    }
    catch(const std::exception& ex) {
      logger->debug("std::exception: {}", ex.what());
      throw;
    }
  }
} /* namespace usbguard */
