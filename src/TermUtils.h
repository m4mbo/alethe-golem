//
// Created by Martin Blicha on 04.08.20.
//

#ifndef OPENSMT_TERMUTILS_H
#define OPENSMT_TERMUTILS_H

#include "osmt_terms.h"

class TermUtils {
    Logic & logic;
public:
    TermUtils(Logic & logic) : logic(logic) {}

    using substitutions_map = std::unordered_map<PTRef, PTRef, PTRefHash>;

    bool isUPOrConstant(PTRef term) const {
        return logic.isUP(term) || (logic.hasSortBool(term) && logic.getPterm(term).nargs() == 0);
    }

    vec<PTRef> getVars(PTRef term) const {
        MapWithKeys<PTRef,bool,PTRefHash> vars;
        ::getVars(term, logic, vars);
        vec<PTRef> keys;
        vars.getKeys().copyTo(keys);
        return keys;
    }

    vec<PTRef> getVarsFromPredicateInOrder(PTRef predicate) const {
        assert(isUPOrConstant(predicate));
        vec<PTRef> vars;
        // iterate over children, all of them should be variables
        auto size = logic.getPterm(predicate).size();
        vars.capacity(size);
        for (int i = 0; i < size; ++i) {
            PTRef var = logic.getPterm(predicate)[i];
            assert(logic.isVar(var));
            vars.push(var);
        }
        return vars;
    }

    PTRef varSubstitute(PTRef term, substitutions_map const & subMap) const {
        MapWithKeys<PTRef, PTRef, PTRefHash> map;
        for (auto const & entry : subMap) {
            map.insert(entry.first, entry.second);
        }
        return Substitutor(logic, map).rewrite(term);
    }

    void printDefine(std::ostream & out, PTRef function, PTRef definition) const {
        out << "(define " << logic.printTerm(function) << ' ' << logic.printTerm(definition) << ")\n";
    }


    struct Conjunction {
        static bool isCorrectJunction(Logic & logic, PTRef term) { return logic.isAnd(term); }
        static bool isOtherJunction(Logic & logic, PTRef term) { return logic.isOr(term); }
    };

    struct Disjunction {
        static bool isCorrectJunction(Logic & logic, PTRef term) { return logic.isOr(term); }
        static bool isOtherJunction(Logic & logic, PTRef term) { return logic.isAnd(term); }
    };

    template<typename Junction, typename TPred>
    vec<PTRef> getTopLevelJuncts(PTRef root, TPred predicate) const {
        // Inspired by Logic::getNewFacts
        vec<PTRef> res;
        Map<PtAsgn,bool,PtAsgnHash> isdup;
        vec<PtAsgn> queue;
        {
            PTRef p;
            lbool sign;
            logic.purify(root, p, sign);
            queue.push(PtAsgn(p, sign));
        }
        while (queue.size() != 0) {
            PtAsgn pta = queue.last(); queue.pop();
            if (isdup.has(pta)) continue;
            isdup.insert(pta, true);
            Pterm const & t = logic.getPterm(pta.tr);
            if (Junction::isCorrectJunction(logic, pta.tr) and pta.sgn == l_True) {
                for (int i = 0; i < t.size(); i++) {
                    PTRef c;
                    lbool c_sign;
                    logic.purify(t[i], c, c_sign);
                    queue.push(PtAsgn(c, c_sign));
                }
            } else if (Junction::isOtherJunction(logic, pta.tr) and (pta.sgn == l_False)) {
                for (int i = 0; i < t.size(); i++) {
                    PTRef c;
                    lbool c_sign;
                    logic.purify(t[i], c, c_sign);
                    queue.push(PtAsgn(c, c_sign ^ true));
                }
            } else {
                PTRef term = pta.sgn == l_False ? logic.mkNot(pta.tr) : pta.tr;
                if (predicate(term)) {
                    res.push(term);
                }
            }
        }
        return res;
    }

    template<typename TPred>
    vec<PTRef> getTopLevelConjuncts(PTRef root, TPred predicate) const {
        return getTopLevelJuncts<Conjunction>(root, predicate);
    }

    vec<PTRef> getTopLevelConjuncts(PTRef root) const {
        return getTopLevelConjuncts(root, [](PTRef){ return true; });
    }

    template<typename TPred>
    vec<PTRef> getTopLevelDisjuncts(PTRef root, TPred predicate) const {
        return getTopLevelJuncts<Disjunction>(root, predicate);
    }

    vec<PTRef> getTopLevelDisjuncts(PTRef root) const {
        return getTopLevelDisjuncts(root, [](PTRef){ return true; });
    }

    PTRef conjoin (PTRef what, PTRef to);

    void insertVarPairsFromPredicates(PTRef domain, PTRef codomain, substitutions_map & subst) const {
        assert(isUPOrConstant(domain) and isUPOrConstant(codomain));
        auto domainVars = getVarsFromPredicateInOrder(domain);
        auto codomainVars = getVarsFromPredicateInOrder(codomain);
        assert(domainVars.size() == codomainVars.size());
        for (int i = 0; i < domainVars.size(); ++i) {
            assert(logic.isVar(domainVars[i]) and logic.isVar(codomainVars[i]));
            subst.insert({domainVars[i], codomainVars[i]});
        }
    }

    void printTermWithLets(ostream & out, PTRef term);

    PTRef simplifyMax(PTRef root) {
        if (logic.isAnd(root) or logic.isOr(root)) {
            root = ::rewriteMaxArityAggresive(logic, root);
            return ::simplifyUnderAssignment_Aggressive(root, logic);
        }
        return root;
    }

    PTRef toNNF(PTRef fla);
};

class LATermUtils {
    LALogic & logic;
public:
    LATermUtils(LALogic & logic) : logic(logic) {}


    /**
     * Given a term 't' and a var 'v' present in the term, returns a term 's' such that
     * 'v = s' is equivalent to 't = 0'
     *
     * @param zeroTerm term that is equal to 0
     * @param var variable present in the term
     * @return term 's' such that 'var = s' is equivalent to 'zeroTerm = 0'
     */
    PTRef expressZeroTermFor(PTRef zeroTerm, PTRef var);

    bool atomContainsVar(PTRef atom, PTRef var);
    bool termContainsVar(PTRef term, PTRef var);

    PTRef simplifyDisjunction(PTRef fla);
    void simplifyDisjunction(std::vector<PtAsgn> & disjuncts);

    PTRef simplifyConjunction(PTRef fla);
    void simplifyConjunction(std::vector<PtAsgn> & disjuncts);
};

class TimeMachine {
    Logic & logic;
    const std::string versionSeparator = "##";

    class VersioningConfig : public DefaultRewriterConfig {
        TimeMachine & owner;
        Logic const & logic;
        int versioningNumber = 0;
    public:
        VersioningConfig(TimeMachine & machine, Logic const & logic) : owner(machine), logic(logic) {}

        void setVersioningNumber(int number) { versioningNumber = number; }

        PTRef rewrite(PTRef term) override {
            if (logic.isVar(term)) {
                return owner.sendVarThroughTime(term, versioningNumber);
            }
            return term;
        }
    };

    class VersioningRewriter : public Rewriter<VersioningConfig> {
    public:
        VersioningRewriter(Logic & logic, VersioningConfig & config) : Rewriter<VersioningConfig>(logic, config) {}
    };

    VersioningConfig config;

public:
    TimeMachine(Logic & logic) : logic(logic), config(*this, logic) {}
    // Returns version of var 'steps' steps in the future (if positive) or in the past (if negative)
    PTRef sendVarThroughTime(PTRef var, int steps) const {
        assert(logic.isVar(var));
        assert(isVersioned(var));
        std::string varName = logic.getSymName(var);
        auto pos = varName.rfind(versionSeparator);
        auto numPos = pos + versionSeparator.size();
        auto numStr = varName.substr(numPos);
        int version = std::stoi(numStr);
        version += steps;
        varName.replace(numPos, std::string::npos, std::to_string(version));
        return logic.mkVar(logic.getSortRef(var), varName.c_str());
    }

    // Given a variable with no version, compute the zero version representing current state
    PTRef getVarVersionZero(PTRef var) {
        assert(logic.isVar(var));
        assert(not isVersioned(var));
        SRef sort = logic.getSortRef(var);
        std::stringstream ss;
        ss << logic.getSymName(var) << versionSeparator << 0;
        std::string newName = ss.str();
        return logic.mkVar(sort, newName.c_str());
    }

    int getVersionNumber(PTRef var) {
        assert(logic.isVar(var));
        assert(isVersioned(var));
        std::string varName = logic.getSymName(var);
        auto pos = varName.rfind(versionSeparator);
        auto numPos = pos + versionSeparator.size();
        auto numStr = varName.substr(numPos);
        int version = std::stoi(numStr);
        return version;
    }

    PTRef sendFlaThroughTime(PTRef fla, int steps) {
        if (steps == 0) { return fla; }
        config.setVersioningNumber(steps);
        VersioningRewriter rewriter(logic, config);
        return rewriter.rewrite(fla);
    }

    bool isVersioned(PTRef var) const {
        assert(logic.isVar(var));
        std::string varName = logic.getSymName(var);
        auto pos = varName.rfind(versionSeparator);
        return pos != std::string::npos;
    }
};

class CanonicalPredicateRepresentation {
    using RepreType = std::unordered_map<SymRef, PTRef, SymRefHash>;
    RepreType stateVersion;
    RepreType nextVersion;
public:
    void addRepresentation(SymRef sym, PTRef stateRepre, PTRef nextStateRepre) {
        stateVersion.insert({sym, stateRepre});
        nextVersion.insert({sym, nextStateRepre});
    }

    PTRef getStateRepresentation(SymRef sym) const { return stateVersion.at(sym); }
    PTRef getNextStateRepresentation(SymRef sym) const { return nextVersion.at(sym); }
};

class UnableToEliminateQuantifierException : std::exception {
    std::string explanation;
public:
    UnableToEliminateQuantifierException(std::string expl) : explanation(std::move(expl)) {}
    UnableToEliminateQuantifierException() : explanation("") {}
};

class TrivialQuantifierElimination {
    Logic & logic;

    PTRef tryGetSubstitutionFromEquality(PTRef var, PTRef eq) const;
public:
    TrivialQuantifierElimination(Logic & logic) : logic(logic) {}

    PTRef eliminateVars(vec<PTRef> const & vars, PTRef fla) const {
        PTRef current = fla;
        for (PTRef var : vars) {
            current = eliminateVar(var, current);
        }
        return current;
    }

    /** Eliminates variable 'var' from formula 'fla'
     *  Throws exception if unable to eliminate
     */
    PTRef eliminateVar(PTRef var, PTRef fla) const {
        assert(logic.isVar(var));
        if (not logic.isVar(var)) {
            throw std::invalid_argument("Quantifier elimination error: " + std::string(logic.printTerm(var)) + " is not a var!");
        }
//        std::cout << "Eliminating " << logic.printTerm(var) << std::endl;
        // Heuristic 1: if there is a top-level definition of the variable, substitute the var with its definition
        //  a) Collect top-level equalities
        Logic & logic = this->logic;
        auto topLevelEqualities = TermUtils(logic).getTopLevelConjuncts(fla, [&logic](PTRef conjunct) { return logic.isEquality(conjunct); });
        // b) Check if any is a definition for the given variable
        auto it = std::find_if(topLevelEqualities.begin(), topLevelEqualities.end(),
                               [var, &logic](PTRef equality){
            assert(logic.isEquality(equality));
            PTRef lhs = logic.getPterm(equality)[0];
            PTRef rhs = logic.getPterm(equality)[1];
            return lhs == var or rhs == var;
        });
        if (it != topLevelEqualities.end()) {
            // found a simple definition; substitute and return
            PTRef eq = *it;
            PTRef lhs = logic.getPterm(eq)[0];
            PTRef rhs = logic.getPterm(eq)[1];
            assert(lhs == var or rhs == var);
            std::unordered_map<PTRef, PTRef, PTRefHash> subs {{lhs == var ? lhs : rhs, lhs == var ? rhs : lhs}};
            PTRef res = TermUtils(logic).varSubstitute(fla, subs);
            return res;
        }
        // c) check if any equality contains the variable
        it = std::find_if(topLevelEqualities.begin(), topLevelEqualities.end(),
                          [var, &logic](PTRef equality){
                              auto vars = TermUtils(logic).getVars(equality);
                              return std::find(vars.begin(), vars.end(), var) != vars.end();
                          });
        if (it != topLevelEqualities.end()) {
            PTRef subst = tryGetSubstitutionFromEquality(var, *it);
            if (subst != PTRef_Undef) {
                std::unordered_map<PTRef, PTRef, PTRefHash> subs {{var, subst}};
                PTRef res = TermUtils(logic).varSubstitute(fla, subs);
                return res;
            }
        }
        // Unable to eliminate this variable, just return the original formula
        return fla;
//        throw UnableToEliminateQuantifierException("Unable to eliminate variable " + std::string(logic.printTerm(var)) + " from " + std::string(logic.printTerm(fla)));
    }
};


#endif //OPENSMT_TERMUTILS_H