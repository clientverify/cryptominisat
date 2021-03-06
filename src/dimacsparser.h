/*****************************************************************************
MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
CryptoMiniSat -- Copyright (c) 2009 Mate Soos

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
******************************************************************************/

#ifndef DIMACSPARSER_H
#define DIMACSPARSER_H

#include <string.h>
#include "streambuffer.h"
#include "cryptominisat4/cryptominisat.h"

#ifdef USE_ZLIB
#include <zlib.h>
#endif


using namespace CMSat;
using std::vector;

template <class C>
class DimacsParser
{
    public:
        DimacsParser(SATSolver* solver, const std::string& debugLib, unsigned _verbosity);

        template <class T> bool parse_DIMACS(T input_stream);
        uint64_t max_var = std::numeric_limits<uint64_t>::max();

    private:
        bool parse_DIMACS_main(C& in);
        bool readClause(C& in);
        bool parse_and_add_clause(C& in);
        bool parse_and_add_xor_clause(C& in);
        bool match(C& in, const char* str);
        bool printHeader(C& in);
        bool parseComments(C& in, const std::string& str);
        std::string stringify(uint32_t x) const;
        bool parseSolveComment(C& in);
        void write_solution_to_debuglib_file(const lbool ret) const;


        SATSolver* solver;
        const std::string debugLib;
        unsigned verbosity;

        //Stat
        size_t lineNum;

        //Printing partial solutions to debugLibPart1..N.output when "debugLib" is set to TRUE
        uint32_t debugLibPart = 1;

        //Reduce temp overhead
        vector<Lit> lits;
        vector<uint32_t> vars;

        size_t norm_clauses_added = 0;
        size_t xor_clauses_added = 0;
};

#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <complex>
#include "assert.h"

using std::vector;
using std::cout;
using std::endl;

template<class C>
DimacsParser<C>::DimacsParser(
    SATSolver* _solver
    , const std::string& _debugLib
    , unsigned _verbosity
):
    solver(_solver)
    , debugLib(_debugLib)
    , verbosity(_verbosity)
    , lineNum(0)
{}

template<class C>
std::string DimacsParser<C>::stringify(uint32_t x) const
{
    std::ostringstream o;
    o << x;
    return o.str();
}

template<class C>
bool DimacsParser<C>::readClause(C& in)
{
    int32_t parsed_lit;
    uint32_t var;
    for (;;) {
        if (!in.parseInt(parsed_lit, lineNum)) {
            return false;
        }
        if (parsed_lit == 0) {
            break;
        }

        var = abs(parsed_lit)-1;

        if (var > max_var) {
            std::cerr
            << "ERROR! "
            << "Variable requested is too large for DIMACS parser parameter: "
            << var << endl
            << "--> At line " << lineNum+1
            << endl;
            return false;
        }

        if (var >= (1ULL<<28)) {
            std::cerr
            << "ERROR! "
            << "Variable requested is far too large: " << var << endl
            << "--> At line " << lineNum+1
            << endl;
            return false;
        }

        while (var >= solver->nVars()) {
            solver->new_var();
        }
        lits.push_back( (parsed_lit > 0) ? Lit(var, false) : Lit(var, true) );
        if (*in != ' ') {
            std::cerr
            << "ERROR! "
            << "After each literal there must be an empty space!"
            << "--> At line " << lineNum+1 << endl
            << endl;
            return false;
        }
    }

    return true;
}

template<class C>
bool DimacsParser<C>::match(C& in, const char* str)
{
    for (; *str != 0; ++str, ++in)
        if (*str != *in)
            return false;
    return true;
}

template<class C>
bool DimacsParser<C>::printHeader(C& in)
{
    if (match(in, "p cnf")) {
        int num_vars;
        int clauses;

        if (!in.parseInt(num_vars, lineNum)
            || !in.parseInt(clauses, lineNum)
        ) {
            return false;
        }
        if (verbosity >= 1) {
            cout << "c -- header says num vars:   " << std::setw(12) << num_vars << endl;
            cout << "c -- header says num clauses:" <<  std::setw(12) << clauses << endl;
        }
        if (num_vars < 0) {
            std::cerr << "ERROR: Number of variables in header cannot be less than 0" << endl;
            return false;
        }
        if (clauses < 0) {
            std::cerr << "ERROR: Number of clauses in header cannot be less than 0" << endl;
            return false;
        }

        if (solver->nVars() <= (size_t)num_vars) {
            solver->new_vars(num_vars-solver->nVars());
        }
    } else {
        std::cerr
        << "PARSE ERROR! Unexpected char: '" << *in
        << "' in the header, at line " << lineNum+1
        << endl;
        return false;
    }

    return true;
}

template<class C>
bool DimacsParser<C>::parseSolveComment(C& in)
{
    vector<Lit> assumps;
    in.skipWhitespace();
    while(*in != ')') {
        int lit;
        if (!in.parseInt(lit, lineNum)) {
            return false;
        }
        assumps.push_back(Lit(std::abs(lit)-1, lit < 0));
        in.skipWhitespace();
    }

    if (verbosity >= 2) {
        cout
        << "c -----------> Solver::solve() called (number: "
        << std::setw(3) << debugLibPart << ") with assumps :";
        for(Lit lit: assumps) {
            cout << lit << " ";
        }
        cout
        << "<-----------"
        << endl;
    }

    lbool ret = solver->solve(&assumps);

    write_solution_to_debuglib_file(ret);
    debugLibPart++;

    if (verbosity >= 6) {
        cout << "c Parsed Solver::solve()" << endl;
    }
    return true;
}

template<class C>
void DimacsParser<C>::write_solution_to_debuglib_file(const lbool ret) const
{
    //Open file for writing
    std::string s = debugLib + "-debugLibPart" + stringify(debugLibPart) +".output";
    std::ofstream partFile;
    partFile.open(s.c_str());
    if (!partFile) {
        std::cerr << "ERROR: Cannot open part file '" << s << "'";
        std::exit(-1);
    }

    //Output to part file the result
    if (ret == l_True) {
        partFile << "s SATISFIABLE\n";
        partFile << "v ";
        for (uint32_t i = 0; i != solver->nVars(); i++) {
            if (solver->get_model()[i] != l_Undef)
                partFile
                << ((solver->get_model()[i]==l_True) ? "" : "-")
                << (i+1) <<  " ";
        }
        partFile << "0\n";
    } else if (ret == l_False) {
        partFile << "conflict ";
        for (Lit lit: solver->get_conflict()) {
            partFile << lit << " ";
        }
        partFile
        << "\ns UNSAT\n";
    } else if (ret == l_Undef) {
        cout << "c timeout, exiting" << endl;
        std::exit(15);
    } else {
        assert(false);
    }
    partFile.close();
}

template<class C>
bool DimacsParser<C>::parseComments(C& in, const std::string& str)
{
    if (!debugLib.empty() && str.substr(0, 13) == "Solver::solve") {
        if (!parseSolveComment(in)) {
            return false;
        }
    } else if (!debugLib.empty() && str == "Solver::new_var()") {
        solver->new_var();

        if (verbosity >= 6) {
            cout << "c Parsed Solver::new_var()" << endl;
        }
    } else if (!debugLib.empty() && str == "Solver::new_vars(") {
        in.skipWhitespace();
        int n;
        if (!in.parseInt(n, lineNum)) {
            return false;
        }
        solver->new_vars(n);

        if (verbosity >= 6) {
            cout << "c Parsed Solver::new_vars( " << n << " )" << endl;
        }
    } else {
        if (verbosity >= 6) {
            cout
            << "didn't understand in CNF file comment line:"
            << "'c " << str << "'"
            << endl;
        }
    }
    in.skipLine();
    lineNum++;
    return true;
}

template<class C>
bool DimacsParser<C>::parse_and_add_clause(C& in)
{
    lits.clear();
    if (!readClause(in)) {
        return false;
    }
    if (!in.skipEOL(lineNum)) {
        return false;
    }
    lineNum++;
    solver->add_clause(lits);
    norm_clauses_added++;
    return true;
}

template<class C>
bool DimacsParser<C>::parse_and_add_xor_clause(C& in)
{
    lits.clear();
    if (!readClause(in)) {
        return false;
    }
    if (!in.skipEOL(lineNum)) {
        return false;
    }
    lineNum++;
    if (lits.empty())
        return true;

    bool rhs = true;
    vars.clear();
    for(Lit& lit: lits) {
        vars.push_back(lit.var());
        if (lit.sign()) {
            rhs ^= true;
        }
    }
    solver->add_xor_clause(vars, rhs);
    xor_clauses_added++;
    return true;
}

template<class C>
bool DimacsParser<C>::parse_DIMACS_main(C& in)
{
    std::string str;

    for (;;) {
        in.skipWhitespace();
        switch (*in) {
        case EOF:
            return true;
        case 'p':
            if (!printHeader(in)) {
                return false;
            }
            in.skipLine();
            lineNum++;
            break;
        case 'c':
            ++in;
            in.parseString(str);
            if (!parseComments(in, str)) {
                return false;
            }
            break;
        case 'x':
            ++in;
            if (!parse_and_add_xor_clause(in)) {
                return false;
            }
            break;
        case '\n':
            std::cerr
            << "c WARNING: Empty line at line number " << lineNum+1
            << " -- this is not part of the DIMACS specifications. Ignoring."
            << endl;
            in.skipLine();
            lineNum++;
            break;
        default:
            if (!parse_and_add_clause(in)) {
                return false;
            }
            break;
        }
    }

    return true;
}

template <class C>
template <class T>
bool DimacsParser<C>::parse_DIMACS(T input_stream)
{
    debugLibPart = 1;
    const uint32_t origNumVars = solver->nVars();

    C in(input_stream);
    if ( !parse_DIMACS_main(in)) {
        return false;
    }

    if (verbosity >= 1) {
        cout
        << "c -- clauses added: " << norm_clauses_added << endl
        << "c -- xor clauses added: " << xor_clauses_added << endl
        << "c -- vars added " << (solver->nVars() - origNumVars)
        << endl;
    }

    return true;
}

#endif //DIMACSPARSER_H
