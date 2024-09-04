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

     //main targets
    for (string target:targets) {
        pkgCache::PkgIterator pkg;
        string vname;

        try {
            pkg = find_package(target);
        }
        catch (runtime_error* e) {

            //package does not exists
            bad_targets.push_back(target);
            continue;
        }

        if (is_virtual(pkg)) {
            try {
                vname = resolve_provide(target);
            }
            catch (runtime_error* e) {

                //no one provide this virtual package
                bad_targets.push_back(target);
                continue;
            }

            //using provided name
            pkg = find_package(vname);
        }

        depmap[pkg.Name ()] = "";
        clog << "[ 0]->" << pkg.Name() << endl;
        build (pkg.VersionList(), 1);
    }

     map <string,pkgCache::DepIterator> newdep;

    /* solving multiple choices */
    recompute_multiples:

    clog << "* multiple choices:" << endl;

    for (pkgCache::DepIterator q:multiples) {
        pkgCache::DepIterator dep = q;

        clog << "* (";

        while (true) {
            string pkgname = dep.TargetPkg().Name();
            clog << pkgname;

            if ((dep->CompareOp & pkgCache::Dep::Or) !=
                pkgCache::Dep::Or) {
                break;
            }

            dep++;
            clog << " | ";
        }
        clog << ")" << endl;
    }

    for (pkgCache::DepIterator q:multiples) {
        pkgCache::DepIterator dep = q;

        bool found = false;

        while (true) {

            string pkgname = dep.TargetPkg().Name();

            if (depmap.find(pkgname) != depmap.end()) {
                clog << "Using " << pkgname << " already included" << endl;

                found = true;
                break;
            }

            if (bootstrap.find(pkgname) != bootstrap.end()) {
                clog << "Using " << pkgname << " from bootstrap" << endl;
                found = true;
                break;
            }

            if ((dep->CompareOp & pkgCache::Dep::Or) !=
                pkgCache::Dep::Or) {
                break;
            }

            dep++;
        }

        if (!found) {

            /*
                no choice has be done, let's look for a
                not banned option
                */

            pkgCache::DepIterator t = q;

            while (true) {

                string pkgname = t.TargetPkg().Name();

                if (banned_targets.find(pkgname) == banned_targets.end()) {
                    if (newdep.find(pkgname) == newdep.end()) {
                        clog << "Adding " << pkgname << endl;
                        newdep[pkgname] = t;
                        break;
                    }
                    else {
                        //already queued for resolve
                        break;
                    }
                }
                else {
                    clog << "Avoiding " << pkgname << endl;
                }

                if ((t->CompareOp & pkgCache::Dep::Or) !=
                    pkgCache::Dep::Or) {
                    break;
                }

                t++;
            }
        }
    }

    multiples.clear ();

    for (pair < string, pkgCache::DepIterator > q:newdep) {
        clog << "* RECOMPUTING: " << q.first << endl;

        pkgCache::PkgIterator pkg = q.second.TargetPkg();

        if (is_virtual(pkg)) {
            try {
                string pkgname = pkg.Name();
                string vname = resolve_provide(pkgname);
                clog << pkgname << " is a virtual package, using " <<
                    vname << endl;

                if (bootstrap.find(vname) == bootstrap.end()) {
                    if (depmap.find(vname) == depmap.end()) {
                        pkgCache::PkgIterator provider = find_package(vname);

                        depmap[vname] = "";
                        build (provider.VersionList(), 0);
                    }
                }
            }
            catch (runtime_error* e) {
                clog << "Could not find a provide for: " << pkg.Name() << endl;
                clog << pkg.Name() << " has been banned" << endl;
                banned_targets.insert(pkg.Name());
                multiples.push_back(q.second);
            }
        }
        else {
            depmap[pkg.Name()] = "";
            build (pkg.VersionList(), 0);
        }
    }

    if (newdep.size() > 0) {
        newdep.clear();
        goto recompute_multiples;
    }

    //does it ever happen?
    clog << "Missing multiples:" << multiples.size() << endl;

    if (bad_targets.size() > 0) {
        clog << "Bad inputs:" << endl;

        for (string target:bad_targets) {
            clog << "* " << target << endl;
        }
    }

    //bootstrap count is included too
    int total = depmap.size();
    if (add_bootstrap) {
        total += bootstrap.size();
    }

    clog << "Total:" << total << endl;

    if (dump_provide) {
        if (prvmap.size () > 0) {
            clog << "Provide : Provided from" << endl;

            for (pair < string, vector < string > >p:prvmap) {
                clog << "* " << p.first << " : ";
                for (string s:p.second) {
                    clog << s << " ";
                }
                clog << endl;
            }
            clog << "Total provides: " << prvmap.size() << endl;
        }
    }

    cout<<"output:";
    for (pair <string,string> q:depmap) {
        cout << q.first << " ";
    }

    if (add_bootstrap) {
        for (pair <string,string> q:bootstrap) {
            cout << q.first << " ";
        }
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

void Solver::build(pkgCache::VerIterator ver, int depth)
{
    bool last_or = false;

    for (pkgCache::DepIterator dep = ver.DependsList(); !dep.end(); dep++) {

        if (dep->Type == pkgCache::Dep::Depends ||
            dep->Type == pkgCache::Dep::Recommends ||
            dep->Type == pkgCache::Dep::PreDepends) {

            //deferred resolution of multiple choices
            if ((dep->CompareOp & pkgCache::Dep::Or) ==
            pkgCache::Dep::Or) {
                if (!last_or) {
                    last_or = true;
                    multiples.push_back(dep);
                }
                continue;
            }
            else {
                if (last_or) {
                    last_or = false;
                    continue;
                }
            }

            pkgCache::PkgIterator pkg = dep.TargetPkg();
            string pkgname = pkg.Name();

            if (is_virtual(pkg)) {
                try {
                    string vname = resolve_provide (pkgname);
                    clog << pkgname <<
                        " is a virtual package, using " << vname << endl;

                    if (bootstrap.find(vname) == bootstrap.end()) {
                        if (depmap.find(vname) == depmap.end()) {
                            pkgCache::PkgIterator provider = find_package(vname);

                            depmap[vname] = "";
                            clog << "[" << setw (2) << depth << "]";
                            for (int n = 0; n < depth; n++) {
                                cout << "-";
                            }
                            clog << "->" << vname << endl;
                            build (provider.VersionList(), depth + 1);
                        }
                    }
                }
                catch (runtime_error* e) {
                    banned_targets.insert(pkgname);
                }
            }
            else {
                if (depmap.find(pkgname) == depmap.end()) {
                    for (pkgCache::VerIterator ver =
                            pkg.VersionList(); !ver.end(); ver++) {

                        if (dep.IsSatisfied(ver)) {

                            /* check against bootstrap package list */
                            if (bootstrap.find(pkgname) ==
                                bootstrap.end()) {
                                depmap[pkgname] = ver.VerStr();
                                clog << "[" << setw (2) << depth << "]";

                                for (int n = 0; n < depth; n++) {
                                    clog << "-";
                                }
                                clog << "->" << pkgname << endl;
                                build (ver, depth + 1);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
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
