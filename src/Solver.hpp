/*
    apt-resolver

    Copyright (C) 2024  Enrique Medina Gremaldos <quique@necos.es>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <apt-pkg/init.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>

#include <string>
#include <map>
#include <vector>

namespace edupals
{
    enum class Option
    {
        None = 0,
        UseInput = 1,
        UseOutput = 2,
        UseBanned = 4,
        Verbose = 8,
        DumpProvide = 16,
        AddBootstrap = 32,
        ComputeBootstrap = 64
    };

    class Solver
    {
        public:

        Solver(int argc,char* argv[]);

        int run();

        void print_help();

        protected:

        std::vector<std::string> m_args;
    };
}
