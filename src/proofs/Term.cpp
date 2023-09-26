/*
 * Copyright (c) 2023, Matias Barandiaran <matias.barandiaran03@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "Term.h"
#include "FastRational.h"
#include <cassert>
#include <memory>
#include <string>

bool Op::nonLinearity() {
    int predicates = 0;
    if (operation == "and") {
        for (auto const & arg : args) {
            if (arg->getTermType() == Term::APP or arg->getTerminalType() == Terminal::VAR) { predicates++; }
        }
        if (predicates >= 2) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

std::string Op::nonLinearSimplification() {
    std::stringstream ss;
    if (operation == "and") {
        for (std::size_t i = 0; i < args.size(); i++) {
            ss << "(not " << args[i]->printTerm() << ")";
            if (i != args.size() - 1) { ss << " "; }
        }
        return ss.str();
    } else {
        throw std::logic_error("This is not a non-linear case!");
    }
}

std::string Term::printTerm() {
    PrintVisitor printVisitor;
    this->accept(&printVisitor);
    return printVisitor.getString();
}

void PrintVisitor::visit(Terminal * term) {
    ss << term->getVal();
}

void PrintVisitor::visit(Op * term) {
    ss << "(" << term->getOp();
    for (const auto & arg : term->getArgs()) {
        ss << " ";
        arg->accept(this);
    }
    ss << ")";
}

void PrintVisitor::visit(App * term) {
    ss << "(" << term->getFun();
    for (const std::shared_ptr<Term> & arg : term->getArgs()) {
        ss << " ";
        arg->accept(this);
    }
    ss << ")";
}

void PrintVisitor::visit(Quant * term) {
    ss << "(" << term->getQuant() << " (";
    for (std::size_t i = 0; i < term->getVars().size(); i++) {
        ss << "(";
        term->getVars()[i]->accept(this);
        ss << " ";
        term->getSorts()[i]->accept(this);
        ss << ")";
        if (i + 1 != term->getVars().size()) { ss << " "; }
    }
    ss << ") ";
    term->getCoreTerm()->accept(this);
    ss << ")";
}

void PrintVisitor::visit(Let * term) {
    auto names = term->getTermNames();
    ss << "(let (";
    for (std::size_t i = 0; i < names.size(); i++) {
        ss << "(" << names[i] << " ";
        term->getDeclarations()[i]->accept(this);
        ss << ")";
        if (i + 1 != names.size()) { ss << " "; }
    }
    ss << ") ";
    term->getApplication()->accept(this);
    ss << ")";
}

std::shared_ptr<Term> CongChainVisitor::visit(Terminal * term) {
    return std::make_shared<Terminal>(term->getVal(), term->getType());
}

std::shared_ptr<Term> CongChainVisitor::visit(Op * term) {

    transCase = 0;
    auto args = term->getArgs();
    bool canSimplify = true;

    for (auto arg : args) {
        if (not(arg->getTermType() == Term::TERMINAL or arg->getTermType() == Term::APP)) {
            canSimplify = false;
            break;
        }
    }

    if (canSimplify) {
        std::vector<int> premises;
        auto simplification = term->accept(&operateVisitor);
        steps.emplace_back(currStep,
                           std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{term->accept(&copyVisitor),
                                                                                        term->accept(&operateVisitor)}),
                           premises, term->simplifyRule());
        currStep++;
        if (term->getOp() == ">") {
            auto originalSimplificaiton = simplification->accept(&copyVisitor);
            auto lessOrEq = std::dynamic_pointer_cast<Op>(simplification)->getArgs()[0];
            auto innerWorking = lessOrEq->accept(&operateVisitor);
            steps.emplace_back(currStep,
                               std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{lessOrEq, innerWorking}),
                               premises, std::dynamic_pointer_cast<Op>(lessOrEq)->simplifyRule());
            currStep++;
            std::dynamic_pointer_cast<Op>(simplification)->setArg(0, innerWorking);
            auto cong =
                std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{originalSimplificaiton, simplification});

            steps.emplace_back(currStep, cong, std::vector<int>{currStep - 1}, "cong");
            currStep++;
            auto outerWorking = simplification->accept(&operateVisitor);
            steps.emplace_back(
                currStep, std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{simplification, outerWorking}),
                premises, std::dynamic_pointer_cast<Op>(simplification)->simplifyRule());

            currStep++;
            auto trans =
                std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{originalSimplificaiton, outerWorking});
            steps.emplace_back(currStep, trans, std::vector<int>{currStep - 2, currStep - 1}, "trans");
            currStep++;
            trans =
                std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{term->accept(&copyVisitor), outerWorking});
            steps.emplace_back(currStep, trans, std::vector<int>{currStep - 5, currStep - 1}, "trans");
            currStep++;
            simplification = outerWorking;
        } else if (term->getOp() == ">=") {
            transCase = 1;
            auto originalSimplificaiton = simplification->accept(&copyVisitor);
            simplification = simplification->accept(&operateVisitor);
            steps.emplace_back(
                currStep,
                std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{originalSimplificaiton, simplification}),
                premises, std::dynamic_pointer_cast<Op>(originalSimplificaiton)->simplifyRule());
            currStep++;
            auto trans = std::make_shared<Op>(
                "=", std::vector<std::shared_ptr<Term>>{term->accept(&copyVisitor), simplification});
            steps.emplace_back(currStep, trans, std::vector<int>{currStep - 2, currStep - 1}, "trans");
            currStep++;
        }
        return simplification;
    } else {

        std::vector<int> premises;
        auto originalTerm = term->accept(&copyVisitor);
        for (int i = 0; i < args.size(); i++) {
            term->setArg(i, args[i]->accept(this));
            if (args[i]->getTermType() == Term::OP) { premises.push_back(currStep - 1); }
        }
        auto cong =
            std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{originalTerm, term->accept(&copyVisitor)});
        steps.emplace_back(currStep, cong, premises, "cong");
        currStep++;
        auto furtherSimplification = term->accept(this);
        auto trans = std::make_shared<Op>("=", std::vector<std::shared_ptr<Term>>{originalTerm, furtherSimplification});
        int predecessor;
        if (transCase == 1) {
            predecessor = currStep - 4;
            transCase = 0;
        } else {
            predecessor = currStep - 2;
        }
        steps.emplace_back(currStep, trans, std::vector<int>{predecessor, currStep - 1}, "trans");
        currStep++;
        return furtherSimplification;
    }
}

std::shared_ptr<Term> CongChainVisitor::visit(App * term) {
    return std::make_shared<App>(term->getFun(), term->getArgs());
}

std::string Op::simplifyRule() {
    std::string op = operation;
    if (op == "=") {
        if ((args[0]->printTerm().find_first_not_of("( )-0123456789") == std::string::npos) and
            (args[1]->printTerm().find_first_not_of("( )-0123456789") == std::string::npos)) {
            return "eq_simplify";
        } else {
            return "equiv_simplify";
        }
    } else if ((op == ">") or (op == "<") or (op == "<=") or (op == ">=")) {
        return "comp_simplify";
    } else if (op == "and") {
        return "and_simplify";
    } else if (op == "or") {
        return "or_simplify";
    } else if (op == "+") {
        return "sum_simplify";
    } else if (op == "-") {
        return "minus_simplify";
    } else if (op == "/" or op == "div") {
        return "div_simplify";
    } else if (op == "*") {
        return "prod_simplify";
    } else if (op == "not") {
        return "not_simplify";
    } else if (op == "ite") {
        return "ite_simplify";
    } else if (op == "mod") {
        return "mod_simplify";
    }
    return "Error";
}

std::shared_ptr<Term> InstantiateVisitor::visit(Terminal * term) {
    auto val = term->getVal();
    auto type = term->getType();
    if (type != Term::VAR) { return std::make_shared<Terminal>(val, type); }
    for (const std::pair<std::string, std::string> & pair : instPairs) {
        if (val == pair.first) {
            if (pair.second == "true" or pair.second == "false") {
                return std::make_shared<Terminal>(pair.second, Term::BOOL);
            } else if (pair.second.find('.') != std::string::npos) {
                return std::make_shared<Terminal>(pair.second, Term::REAL);
            } else {
                return std::make_shared<Terminal>(pair.second, Term::INT);
            }
        }
    }
    return std::make_shared<Terminal>(val, type);
}

std::shared_ptr<Term> InstantiateVisitor::visit(Op * term) {
    std::vector<std::shared_ptr<Term>> args;
    std::string opcode = term->getOp();
    for (const std::shared_ptr<Term> & arg : term->getArgs()) {
        args.push_back(arg->accept(this));
    }
    return std::make_shared<Op>(opcode, args);
}

std::shared_ptr<Term> InstantiateVisitor::visit(App * term) {
    std::vector<std::shared_ptr<Term>> args;
    std::string fun = term->getFun();
    for (std::shared_ptr<Term> arg : term->getArgs()) {
        args.push_back(arg->accept(this));
    }
    return std::make_shared<App>(fun, args);
}

std::shared_ptr<Term> InstantiateVisitor::visit(Quant * term) {
    std::shared_ptr<Term> coreTerm = term->getCoreTerm()->accept(this);
    return coreTerm;
}

std::shared_ptr<Term> InstantiateVisitor::visit(Let * term) {
    std::vector<std::shared_ptr<Term>> declarations;
    auto application = term->getApplication();
    for (std::shared_ptr<Term> dec : term->getDeclarations()) {
        declarations.push_back(dec->accept(this));
    }
    application->accept(this);
    return std::make_shared<Let>(term->getTermNames(), declarations, application);
}

std::shared_ptr<Term> RemoveUnusedVisitor::visit(Quant * term) {

    // one pass colection
    auto vars = term->getVars();
    auto sorts = term->getSorts();
    auto coreTerm = term->getCoreTerm();
    bool inUse = false;

    coreTerm->accept(this);

    for (std::size_t i = 0; i < vars.size(); i++) {
        auto var = vars[i];
        for (auto varInUse : varsInUse) {
            if (var->printTerm() == varInUse) { inUse = true; }
        }
        if (!inUse) {
            vars.erase(vars.begin() + i);
            sorts.erase(sorts.begin() + i);
        }
        inUse = false;
    }

    if (vars.empty()) { return term->getCoreTerm(); }
    return std::make_shared<Quant>(term->getQuant(), vars, sorts, term->getCoreTerm());
}

std::shared_ptr<Term> RemoveUnusedVisitor::visit(Terminal * term) {
    for (auto var : varsInUse) {
        if (term->printTerm() == var) { return nullptr; }
    }
    varsInUse.push_back(term->printTerm());
    return nullptr;
}

std::shared_ptr<Term> RemoveUnusedVisitor::visit(Op * term) {
    auto args = term->getArgs();
    for (auto arg : args) {
        arg->accept(this);
    }
    return nullptr;
}

std::shared_ptr<Term> RemoveUnusedVisitor::visit(App * term) {
    auto args = term->getArgs();
    for (auto arg : args) {
        arg->accept(this);
    }
    return nullptr;
}

std::shared_ptr<Term> SimplifyVisitor::visit(Terminal * term) {
    return std::make_shared<Terminal>(term->getVal(), term->getType());
}

std::shared_ptr<Term> SimplifyVisitor::visit(Op * term) {

    auto args = term->getArgs();
    std::vector<std::shared_ptr<Term>> newArgs;
    auto op = term->getOp();

    if (operation == term) {
        return simplification;
    } else {
        for (auto arg : args) {
            newArgs.push_back(arg->accept(this));
        }
        return std::make_shared<Op>(op, newArgs);
    }
}

std::shared_ptr<Term> SimplifyVisitor::visit(App * term) {
    return std::make_shared<App>(term->getFun(), term->getArgs());
}

std::shared_ptr<Term> SimplifyVisitor::visit(Quant * term) {

    return std::make_shared<Quant>(term->getQuant(), term->getVars(), term->getSorts(),
                                   term->getCoreTerm()->accept(this));
}

std::shared_ptr<Term> SimplifyVisitor::visit(Let * term) {

    if (operation == term) {
        return simplification;
    } else {
        return std::make_shared<Let>(term->getTermNames(), term->getDeclarations(),
                                     term->getApplication()->accept(this));
    }
}

std::shared_ptr<Term> OperateVisitor::visit(Op * term) {
    std::string op = term->getOp();
    std::vector<std::shared_ptr<Term>> args = term->getArgs();
    std::vector<std::shared_ptr<Term>> newArgs;
    InstantiateVisitor fakeInstantiation;
    std::string firstStr;
    std::string secondStr;
    FastRational firstTerm;
    FastRational secondTerm;

    if (op == "<" or op == "<=" or op == "-" or op == "*" or op == "/" or op == "mod" or op == "div") {
        assert(args[0]->getTerminalType() != Term::VAR);
        assert(args[1]->getTerminalType() != Term::VAR);
        firstStr = args[0]->printTerm();
        secondStr = args[1]->printTerm();
        firstStr.erase(remove(firstStr.begin(), firstStr.end(), '('), firstStr.end());
        firstStr.erase(remove(firstStr.begin(), firstStr.end(), ')'), firstStr.end());
        firstStr.erase(remove(firstStr.begin(), firstStr.end(), ' '), firstStr.end());
        secondStr.erase(remove(secondStr.begin(), secondStr.end(), '('), secondStr.end());
        secondStr.erase(remove(secondStr.begin(), secondStr.end(), ')'), secondStr.end());
        secondStr.erase(remove(secondStr.begin(), secondStr.end(), ' '), secondStr.end());
        firstTerm = FastRational(&firstStr[0], 10);
        secondTerm = FastRational(&secondStr[0], 10);
    }

    if (op == "=") {
        assert(args[0]->getTerminalType() != Term::VAR);
        assert(args[1]->getTerminalType() != Term::VAR);
        if (args[0]->printTerm() == args[1]->printTerm()) {
            return std::make_shared<Terminal>("true", Term::BOOL);
        } else {
            return std::make_shared<Terminal>("false", Term::BOOL);
        }
    } else if (op == ">") {
        return std::make_shared<Op>("not", std::vector<std::shared_ptr<Term>>{std::make_shared<Op>("<=", args)});
    } else if (op == "<") {
        if (firstTerm < secondTerm) {
            return std::make_shared<Terminal>("true", Term::BOOL);
        } else {
            return std::make_shared<Terminal>("false", Term::BOOL);
        }
    } else if (op == "<=") {
        if (firstTerm <= secondTerm) {
            return std::make_shared<Terminal>("true", Term::BOOL);
        } else {
            return std::make_shared<Terminal>("false", Term::BOOL);
        }
    } else if (op == ">=") {
        newArgs.push_back(args[1]);
        newArgs.push_back(args[0]);
        return std::make_shared<Op>("<=", newArgs);
    } else if (op == "and") {
        int trues = 0;
        std::vector<std::shared_ptr<Term>> predicates;

        for (auto arg : args) {
            //           assert(arg->getTerminalType() != Term::VAR);
            if (arg->printTerm() == "false") { return std::make_shared<Terminal>("false", Term::BOOL); }
            if (arg->printTerm() == "true") { trues++; }
            if (arg->printTerm() != "true") { predicates.push_back(arg); }
        }
        if (trues == int(args.size())) { return std::make_shared<Terminal>("true", Term::BOOL); }
        if (predicates.size() == 1) {
            return predicates[0]->accept(&fakeInstantiation);
        } else {
            for (auto predicate : predicates) {
                newArgs.push_back(predicate->accept(&fakeInstantiation));
            }
            return std::make_shared<Op>("and", newArgs);
        }
    } else if (op == "or") {
        for (auto arg : args) {
            assert(arg->getTerminalType() != Term::VAR);
            if (arg->printTerm() == "true") { return std::make_shared<Terminal>("true", Term::BOOL); }
        }
        return std::make_shared<Terminal>("false", Term::BOOL);
    } else if (op == "+") {
        FastRational result = 0;
        for (auto arg : args) {
            assert(arg->getTerminalType() != Term::VAR);
            std::string str = arg->printTerm();
            str.erase(remove(str.begin(), str.end(), '('), str.end());
            str.erase(remove(str.begin(), str.end(), ')'), str.end());
            str.erase(remove(str.begin(), str.end(), ' '), str.end());
            const char * ptr = &str[0];
            FastRational temp(ptr, 10);
            result += temp;
        }
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.get_str(), Term::INT);
        }
    } else if (op == "-") {
        FastRational result = firstTerm - secondTerm;
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.get_str(), Term::INT);
        }
    } else if (op == "/") {
        FastRational result = firstTerm / secondTerm;
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.get_str(), Term::INT);
        }
    } else if (op == "*") {
        FastRational result = firstTerm * secondTerm;
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.get_str(), Term::INT);
        }
    } else if (op == "not") {
        assert(args[0]->getTerminalType() != Term::VAR);
        if (args[0]->printTerm() == "false") {
            return std::make_shared<Terminal>("true", Term::BOOL);
        } else {
            return std::make_shared<Terminal>("false", Term::BOOL);
        }
    } else if (op == "ite") {
        assert(args[0]->getTerminalType() != Term::VAR);
        assert(args[1]->getTerminalType() != Term::VAR);
        assert(args[2]->getTerminalType() != Term::VAR);
        if (args[0]->printTerm() == "true") {
            return args[1]->accept(&fakeInstantiation);
        } else {
            return args[2]->accept(&fakeInstantiation);
        }
    } else if (op == "mod") {
        FastRational result = firstTerm % secondTerm;
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.get_str(), Term::INT);
        }
    } else if (op == "div") {
        FastRational result = firstTerm / secondTerm;
        if (result < 0) {
            result *= -1;
            return std::make_shared<Terminal>("(- " + result.ceil().get_str() + ")", Term::INT);
        } else {
            return std::make_shared<Terminal>(result.floor().get_str(), Term::INT);
        }
    }

    return std::make_shared<Terminal>("Error", Term::UNDECLARED);
}

std::shared_ptr<Term> OperateLetTermVisitor::visit(Terminal * term) {

    for (std::size_t i = 0; i < terms.size(); i++) {
        if (term->getVal() == terms[i]) { return substitutions[i]; }
    }
    return std::make_shared<Terminal>(term->getVal(), term->getType());
}

std::shared_ptr<Term> OperateLetTermVisitor::visit(Op * term) {
    std::vector<std::shared_ptr<Term>> args;
    std::string opcode = term->getOp();
    for (std::shared_ptr<Term> arg : term->getArgs()) {
        args.push_back(arg->accept(this));
    }
    return std::make_shared<Op>(opcode, args);
}

std::shared_ptr<Term> OperateLetTermVisitor::visit(App * term) {
    std::vector<std::shared_ptr<Term>> args;
    std::string fun = term->getFun();
    for (std::shared_ptr<Term> arg : term->getArgs()) {
        args.push_back(arg->accept(this));
    }
    return std::make_shared<App>(fun, args);
}

std::shared_ptr<Term> OperateLetTermVisitor::visit(Let * term) {
    terms = term->getTermNames();
    substitutions = term->getDeclarations();
    return term->getApplication()->accept(this);
}

Term * LetLocatorVisitor::visit(Quant * term) {
    return term->getCoreTerm()->accept(this);
}

Term * LetLocatorVisitor::visit(Op * term) {
    auto args = term->getArgs();
    for (auto arg : args) {
        if (arg->accept(this) != nullptr) { return arg->accept(this); }
    }
    return nullptr;
}

Term * LetLocatorVisitor::visit(Let * term) {
    if (term->getApplication()->accept(this) == nullptr) { return term; }
    return term->getApplication()->accept(this);
}

std::string Terminal::accept(StringVisitor * visitor) {
    return visitor->visit(this);
}

std::string Op::accept(StringVisitor * visitor) {
    return visitor->visit(this);
}

std::string App::accept(StringVisitor * visitor) {
    return visitor->visit(this);
}

std::string Quant::accept(StringVisitor * visitor) {
    return visitor->visit(this);
}

std::string Let::accept(StringVisitor * visitor) {
    return visitor->visit(this);
}

std::shared_ptr<Term> Terminal::accept(LogicVisitor * visitor) {
    return visitor->visit(this);
}

std::shared_ptr<Term> Op::accept(LogicVisitor * visitor) {
    return visitor->visit(this);
}

std::shared_ptr<Term> App::accept(LogicVisitor * visitor) {
    return visitor->visit(this);
}

std::shared_ptr<Term> Quant::accept(LogicVisitor * visitor) {
    return visitor->visit(this);
}

std::shared_ptr<Term> Let::accept(LogicVisitor * visitor) {
    return visitor->visit(this);
}

bool Terminal::accept(BooleanVisitor * visitor) {
    return visitor->visit(this);
}

bool Op::accept(BooleanVisitor * visitor) {
    return visitor->visit(this);
}

bool App::accept(BooleanVisitor * visitor) {
    return visitor->visit(this);
}

bool Quant::accept(BooleanVisitor * visitor) {
    return visitor->visit(this);
}

bool Let::accept(BooleanVisitor * visitor) {
    return visitor->visit(this);
}

void Terminal::accept(VoidVisitor * visitor) {
    visitor->visit(this);
}

void Op::accept(VoidVisitor * visitor) {
    visitor->visit(this);
}

void App::accept(VoidVisitor * visitor) {
    visitor->visit(this);
}

void Quant::accept(VoidVisitor * visitor) {
    visitor->visit(this);
}

void Let::accept(VoidVisitor * visitor) {
    visitor->visit(this);
}

Term * Terminal::accept(PointerVisitor * visitor) {
    return visitor->visit(this);
}

Term * Op::accept(PointerVisitor * visitor) {
    return visitor->visit(this);
}

Term * App::accept(PointerVisitor * visitor) {
    return visitor->visit(this);
}

Term * Quant::accept(PointerVisitor * visitor) {
    return visitor->visit(this);
}

Term * Let::accept(PointerVisitor * visitor) {
    return visitor->visit(this);
}
