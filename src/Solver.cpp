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
                    banned_targets.insert(arg);
                    break;

                default:
                    break;
            }
    }

    pkgInitConfig(*_config);
    pkgInitSystem(*_config, _system);

    pkgCacheFile cache_file;
    cache = cache_file.GetPkgCache();

    /* building bootstrap and provide map */
    clog << "* Building cache..." << endl;

    for (pkgCache::PkgIterator pkg = cache->PkgBegin(); !pkg.end(); pkg++) {

        if (is_virtual(pkg)) {
            continue;
        }

        //we just take first available version
        pkgCache::VerIterator ver = pkg.VersionList();

        if (ver->Priority == pkgCache::State::Required ||
            ver->Priority == pkgCache::State::Important) {

            bootstrap[pkg.Name()] = ver.VerStr();
        }

        for (pkgCache::PrvIterator prv = ver.ProvidesList(); !prv.end(); prv++) {

            pkgCache::PkgIterator owner = prv.OwnerPkg();

            string pname = prv.Name();
            string oname = pkg.Name();

            if (pname != oname) {
                prvmap[pname].push_back(oname);
            }
        }
    }

    if (compute_bootstrap) {
        //ignore add bootsrap option
        add_bootstrap = false;
        clog<<"* Adding bootstrap packages..."<<endl;

        for (std::pair<string,string> package : bootstrap) {
            targets.push_back(package.first);
        }
    }

    for (string out:targets) {
        cout<<out<<" ";
    }
    cout<<endl;

    return 0;
}

pkgCache::PkgIterator Solver::find_package(string pkgname)
{
    pkgCache::PkgIterator pkg = cache->FindPkg(pkgname);

    if (pkg.end()) {
        throw runtime_error("package does not exists");
    }

    return pkg;
}

string Solver::resolve_provide(string prvname)
{
    string ret = "";

    if (virtuals.find(prvname) == virtuals.end()) {

        if (prvmap.find(prvname) != prvmap.end()) {
            for (string s:prvmap[prvname]) {
                if (depmap.find(s) != depmap.end()) {
                    ret = s;
                    break;
                }
            }

            if (ret == "") {
                ret = prvmap[prvname][0];
            }
        }

        virtuals[prvname] = ret;
    }
    else {
        ret = virtuals[prvname];
    }

    if (ret == "") {
        throw runtime_error ("Could not find provide " + prvname);
    }

    return ret;
}

bool Solver::is_virtual(pkgCache::PkgIterator pkg)
{
    pkgCache::VerIterator ver = pkg.VersionList();

    return (ver.end());
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
