// Pure C++23 implementation of AWK utility (simplified version)
// Pattern scanning and processing language - No libc, only libc++

#include <algorithm>
#include <expected>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
    struct AwkVariables {
        std::map<std::string, std::string> vars;
        std::vector<std::string> fields;  // $0, $1, $2, ...
        int NF = 0;  // Number of fields
        int NR = 0;  // Number of records
        std::string FS = " ";  // Field separator
        std::string RS = "\n"; // Record separator
        std::string OFS = " ";  // Output field separator
        std::string ORS = "\n"; // Output record separator
        
        [[nodiscard]] std::string get_field(int n) const {
            if (n == 0 && !fields.empty()) {
                return fields[0];  // Whole record
            }
            if (n > 0 && static_cast<size_t>(n) < fields.size()) {
                return fields[n];
            }
            return "";
        }
        
        void set_field(int n, const std::string& value) {
            if (n >= 0 && static_cast<size_t>(n) < fields.size()) {
                fields[n] = value;
                if (n == 0) {
                    split_fields(value);
                } else {
                    rebuild_record();
                }
            }
        }
        
        void split_fields(const std::string& record) {
            fields.clear();
            fields.push_back(record);  // $0
            
            if (FS == " ") {
                // Special case: split on any whitespace
                std::istringstream iss(record);
                std::string field;
                while (iss >> field) {
                    fields.push_back(field);
                }
            } else if (FS.size() == 1) {
                // Single character separator
                for (auto part : record | std::views::split(FS[0])) {
                    fields.emplace_back(part.begin(), part.end());
                }
            } else {
                // Multi-character separator (use regex)
                std::regex sep(FS);
                std::sregex_token_iterator iter(record.begin(), record.end(), sep, -1);
                std::sregex_token_iterator end;
                for (; iter != end; ++iter) {
                    fields.push_back(iter->str());
                }
            }
            
            NF = static_cast<int>(fields.size()) - 1;  // Don't count $0
        }
        
        void rebuild_record() {
            if (fields.size() <= 1) return;
            
            std::ostringstream oss;
            for (size_t i = 1; i < fields.size(); ++i) {
                if (i > 1) oss << OFS;
                oss << fields[i];
            }
            fields[0] = oss.str();
        }
    };
    
    enum class ActionType {
        Print,
        PrintF,
        Assignment,
        Expression
    };
    
    struct Action {
        ActionType type;
        std::string code;
        std::vector<std::string> args;
    };
    
    struct Pattern {
        enum Type { Begin, End, Expression, None } type = None;
        std::string expr;
    };
    
    struct Rule {
        Pattern pattern;
        std::vector<Action> actions;
    };
    
    class SimpleAwk {
    private:
        std::vector<Rule> rules;
        AwkVariables vars;
        
        // Simple expression evaluator
        [[nodiscard]] bool evaluate_condition(const std::string& expr, 
                                             const AwkVariables& context) {
            // Very simplified - just handle basic patterns
            if (expr.empty()) return true;
            
            // Handle regex match: /pattern/
            if (expr.starts_with("/") && expr.ends_with("/")) {
                std::string pattern = expr.substr(1, expr.length() - 2);
                std::regex regex_pattern(pattern);
                return std::regex_search(context.get_field(0), regex_pattern);
            }
            
            // Handle field comparisons: $1 == "value"
            if (expr.contains("==")) {
                auto parts = expr | std::views::split("==") | 
                           std::views::transform([](auto&& part) {
                               std::string s(part.begin(), part.end());
                               // Trim whitespace
                               s.erase(0, s.find_first_not_of(" \t"));
                               s.erase(s.find_last_not_of(" \t") + 1);
                               return s;
                           });
                
                auto it = parts.begin();
                std::string left = *it;
                ++it;
                std::string right = *it;
                
                // Remove quotes from right side
                if (right.starts_with("\"") && right.ends_with("\"")) {
                    right = right.substr(1, right.length() - 2);
                }
                
                // Evaluate field reference
                if (left.starts_with("$")) {
                    int field_num = std::stoi(left.substr(1));
                    return context.get_field(field_num) == right;
                }
                
                return left == right;
            }
            
            // Handle NF, NR comparisons
            if (expr.starts_with("NF")) {
                // Simple NF > 0 type expressions
                return context.NF > 0;
            }
            
            return true;  // Default: match everything
        }
        
        void execute_action(const Action& action, AwkVariables& context) {
            switch (action.type) {
                case ActionType::Print:
                    if (action.args.empty()) {
                        std::cout << context.get_field(0) << context.ORS;
                    } else {
                        for (size_t i = 0; i < action.args.size(); ++i) {
                            if (i > 0) std::cout << context.OFS;
                            
                            // Handle field references
                            if (action.args[i].starts_with("$")) {
                                int field_num = std::stoi(action.args[i].substr(1));
                                std::cout << context.get_field(field_num);
                            } else {
                                std::cout << action.args[i];
                            }
                        }
                        std::cout << context.ORS;
                    }
                    break;
                    
                case ActionType::PrintF:
                    // Simplified printf
                    if (!action.args.empty()) {
                        std::cout << action.args[0];  // Format string as-is for now
                        std::cout.flush();
                    }
                    break;
                    
                case ActionType::Assignment:
                    // Handle variable assignments
                    break;
                    
                case ActionType::Expression:
                    // Evaluate expression
                    break;
            }
        }
        
    public:
        void add_rule(const std::string& pattern_str, const std::string& action_str) {
            Rule rule;
            
            // Parse pattern
            if (pattern_str == "BEGIN") {
                rule.pattern.type = Pattern::Begin;
            } else if (pattern_str == "END") {
                rule.pattern.type = Pattern::End;
            } else if (!pattern_str.empty()) {
                rule.pattern.type = Pattern::Expression;
                rule.pattern.expr = pattern_str;
            }
            
            // Parse action
            Action action;
            if (action_str.empty() || action_str == "print") {
                action.type = ActionType::Print;
            } else if (action_str.starts_with("print ")) {
                action.type = ActionType::Print;
                std::istringstream iss(action_str.substr(6));
                std::string arg;
                while (iss >> arg) {
                    action.args.push_back(arg);
                }
            } else if (action_str.starts_with("printf")) {
                action.type = ActionType::PrintF;
                action.args.push_back(action_str.substr(7));
            } else {
                action.type = ActionType::Expression;
                action.code = action_str;
            }
            
            rule.actions.push_back(action);
            rules.push_back(rule);
        }
        
        void process_file(std::istream& input) {
            // Execute BEGIN rules
            for (const auto& rule : rules) {
                if (rule.pattern.type == Pattern::Begin) {
                    for (const auto& action : rule.actions) {
                        execute_action(action, vars);
                    }
                }
            }
            
            // Process each line
            std::string line;
            while (std::getline(input, line)) {
                vars.NR++;
                vars.split_fields(line);
                
                // Execute pattern rules
                for (const auto& rule : rules) {
                    if (rule.pattern.type == Pattern::None || 
                        (rule.pattern.type == Pattern::Expression &&
                         evaluate_condition(rule.pattern.expr, vars))) {
                        for (const auto& action : rule.actions) {
                            execute_action(action, vars);
                        }
                    }
                }
            }
            
            // Execute END rules
            for (const auto& rule : rules) {
                if (rule.pattern.type == Pattern::End) {
                    for (const auto& action : rule.actions) {
                        execute_action(action, vars);
                    }
                }
            }
        }
    };
}

int main(int argc, char* argv[]) {
    SimpleAwk awk;
    std::vector<std::string> files;
    std::string program;
    
    // Parse arguments using C++23 span
    std::span<char*> args(argv + 1, argc - 1);
    
    if (args.empty()) {
        std::cerr << "Usage: awk 'pattern { action }' [file...]\n";
        return 1;
    }
    
    // First argument is the program
    program = args[0];
    
    // Remaining arguments are files
    for (size_t i = 1; i < args.size(); ++i) {
        files.emplace_back(args[i]);
    }
    
    // Simple program parsing (very basic)
    if (program.contains('{') && program.contains('}')) {
        auto brace_start = program.find('{');
        auto brace_end = program.rfind('}');
        
        std::string pattern = program.substr(0, brace_start);
        std::string action = program.substr(brace_start + 1, 
                                           brace_end - brace_start - 1);
        
        // Trim whitespace
        pattern.erase(0, pattern.find_first_not_of(" \t"));
        pattern.erase(pattern.find_last_not_of(" \t") + 1);
        action.erase(0, action.find_first_not_of(" \t"));
        action.erase(action.find_last_not_of(" \t") + 1);
        
        awk.add_rule(pattern, action);
    } else {
        // Default action is print
        awk.add_rule(program, "print");
    }
    
    // Process files or stdin
    if (files.empty()) {
        awk.process_file(std::cin);
    } else {
        for (const auto& filename : files) {
            if (filename == "-") {
                awk.process_file(std::cin);
            } else {
                std::ifstream file(filename);
                if (!file) {
                    std::cerr << std::format("awk: {}: No such file or directory\n",
                                           filename);
                    return 1;
                }
                awk.process_file(file);
            }
        }
    }
    
    return 0;
}