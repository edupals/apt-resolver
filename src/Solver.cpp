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

#include "Solver.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <set>

using namespace edupals;

using namespace std;

Solver::Solver(int argc,char* argv[])
{
    for (int n = 1;n < argc;n++) {
        m_args.push_back(argv[n]);
    }
}

int Solver::run()
{
    vector <string> targets;
    set<string> bad_targets;

    bool dump_provide = false;
    bool add_bootstrap = false;
    bool compute_bootstrap = false;
    Option option = Option::None;

    if (m_args.size() == 0) {
        print_help();
        return 0;
    }

    for (string arg : m_args) {
            if (arg == "-i") {
                option = Option::UseInput;
                continue;
            }

            if (arg == "-o") {
                option = Option::UseOutput;
                continue;
            }

            if (arg == "-b") {
                option = Option::UseBanned;
                continue;
            }

            if (arg == "-p") {
                option = Option::DumpProvide;
                dump_provide = true;
                continue;
            }

            if (arg == "-d") {
                option = Option::AddBootstrap;
                add_bootstrap = true;
                continue;
            }

            if (arg == "-c") {
                option = Option::ComputeBootstrap;
                compute_bootstrap = true;
                continue;
            }

            switch (option) {

                case Option::UseInput:
                    targets.push_back(arg);
                    break;

                case Option::UseBanned:
                    bad_targets.insert(arg);
                    break;
            }
    }

    return 0;
}

void Solver::print_help()
{
    cout << "Usage: " << endl;
    cout << "apt-resolver [OPTIONS]" << endl;
    cout << endl;

    cout << "Available options:" << endl;
    cout << "-i <targets>\tInput packages" << endl;
    cout << "-d Adds bootstrap to output list" << endl;
    cout << "-c Adds bootstrap to output list recomputing dependency tree. This will ignore -d" << endl;
    cout << "-b <targets>\tPackages to avoid" << endl;
    cout << "-p\tDumps provide list" << endl;
}
