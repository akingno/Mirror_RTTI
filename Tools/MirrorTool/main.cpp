#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>

struct VariableInfo {
    std::string type;
    std::string name;
};

struct ArgInfo {
    std::string type; // 需要知道参数类型进行any_cast
};

struct FunctionInfo {
    std::string retType;
    std::string name;
    std::vector<ArgInfo> args;
};

struct ClassInfo {
    std::string className;
    std::vector<VariableInfo> variables;
    std::vector<FunctionInfo> functions;
};

enum class ParseState {
    Idle, // 寻找类名
    InClass, // 在类内寻找to_reflect
    InReflectZone // 在反射区抓取变量
};

// 剥离注释
std::string StripComments(std::string line) {
    auto pos = line.find("//");
    if (pos != std::string::npos) {
        line = line.substr(0, pos);
    }
    return line;
}

std::vector<ArgInfo> ParseArgs(const std::string &argsStr) {
    std::vector<ArgInfo> result;
    if (argsStr.empty() || argsStr == " ") return result;

    std::stringstream ss(argsStr);
    std::string item;
    std::regex argRegex(R"(^\s*([\w\*:]+))");

    while (std::getline(ss, item, ',')) {
        std::smatch m;
        if (std::regex_search(item, m, argRegex)) {
            result.push_back({m[1].str()});
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: MirrorTool <head file path> <output .cpp path>\n";
        return -1;
    }
    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

    std::ifstream inFile(inputFile);
    if (!inFile) {
        std::cerr << "Can not open the input file: " << inputFile << "\n";
        return -1;
    }

    std::string line;
    ParseState state = ParseState::Idle;
    ClassInfo currentClass;
    std::vector<ClassInfo> parsedClasses;

    std::regex classRegex(R"(^\s*reflect_struct\s+(\w+))");
    std::regex toReflectRegex(R"(^\s*to_reflect)");
    std::regex noReflectRegex(R"(^\s*no_reflect)");
    std::regex varRegex(R"(^\s*([\w\*:]+)\s+(\w+)\s*(?:=.*)?;\s*)");
    std::regex funcRegex(R"(^\s*([\w\*:]+)\s+(\w+)\s*\((.*?)\)\s*;\s*)");

    std::cout << "MirrorTool: Scanning Begin" << inputFile << " ...\n";

    while (std::getline(inFile, line)) {
        line = StripComments(line);
        if (line.empty()) continue;

        std::smatch match;

        if (state == ParseState::Idle) {
            // 找类名
            if (std::regex_search(line, match, classRegex)) {
                currentClass.className = match[1].str();
                currentClass.variables.clear();
                state = ParseState::InClass;
                std::cout << "Class found: " << currentClass.className << "\n";
            }
        } else if (state == ParseState::InClass) {
            // 找反射区起点
            if (std::regex_search(line, match, toReflectRegex)) {
                state = ParseState::InReflectZone;
            } else if (line.find("}") != std::string::npos) {
                // 如果没写to_reflect就结束了，也保存
                parsedClasses.push_back(currentClass);
                state = ParseState::Idle;
            }
        } else if (state == ParseState::InReflectZone) {
            // 找反射区终点
            if (std::regex_search(line, match, noReflectRegex)) {
                state = ParseState::InClass;
            } else if (line.find("}") != std::string::npos) {
                parsedClasses.push_back(currentClass);
                state = ParseState::Idle;
            }
            // 匹配函数（带有括号）
            else if (std::regex_search(line, match, funcRegex)) {
                FunctionInfo func;
                func.retType = match[1].str();
                func.name = match[2].str();
                func.args = ParseArgs(match[3].str());
                currentClass.functions.push_back(func);
                std::cout << "    -> 抓取函数: " << func.retType << " " << func.name << "(参数个数: " << func.args.size() <<
                        ")\n";
            }
            // 后匹配变量
            else if (std::regex_search(line, match, varRegex)) {
                VariableInfo var = {match[1].str(), match[2].str()};
                currentClass.variables.push_back(var);
                std::cout << "    -> 抓取变量: " << var.type << " " << var.name << "\n";
            }
        }
    }

    // 开始生成
    std::ofstream outFile(outputFile);
    outFile << "// ==================================================================\n";
    outFile << "// 本文件由 MirrorTool 自动生成，请勿手动修改.\n";
    outFile << "// ==================================================================\n\n";
    outFile << "#include \"Mirror_RTTI.h\"\n";
    outFile << "#include \"" << inputFile << "\"\n\n";

    outFile << "namespace {\n";

    for (const auto &cls: parsedClasses) {
        outFile << "    struct " << cls.className << "_AutoRegistrar {\n";
        outFile << "        " << cls.className << "_AutoRegistrar() {\n";

        outFile << "            auto& desc = ReflectionRegistry::Instance().RegisterClass(typeid("
                << cls.className << "), \"" << cls.className << "\",\n"
                << "                []() -> void* { return new " << cls.className << "(); },\n"
                << "                [](void* p) { delete static_cast<" << cls.className << "*>(p); }\n"
                << "            );\n\n";

        for (const auto &var: cls.variables) {
            outFile << "            desc.AddField(\"" << var.name << "\", typeid("
                    << var.type << "), offsetof(" << cls.className << ", " << var.name << "));\n";
        }

        for (const auto &func: cls.functions) {
            outFile << "            desc.AddFunction(\"" << func.name <<
                    "\", [](void* instance, const std::vector<std::any>& args) -> std::any {\n";
            outFile << "                auto* obj = static_cast<" << cls.className << "*>(instance);\n";

            std::string callArgs = "";
            for (size_t i = 0; i < func.args.size(); ++i) {
                callArgs += "std::any_cast<" + func.args[i].type + ">(args[" + std::to_string(i) + "])";
                if (i < func.args.size() - 1) callArgs += ", ";
            }

            if (func.retType == "void") {
                outFile << "                obj->" << func.name << "(" << callArgs << ");\n";
                outFile << "                return std::any();\n";
            } else {
                outFile << "                return obj->" << func.name << "(" << callArgs << ");\n";
            }
            outFile << "            });\n\n";
        }

        outFile << "        }\n";
        outFile << "    };\n";
        outFile << "    static " << cls.className << "_AutoRegistrar global_" << cls.className << "_reg_instance;\n";
    }

    outFile << "}\n";

    std::cout << "MirrorTool: 代码生成完毕。已写入 " << outputFile << "\n";
    return 0;
}
