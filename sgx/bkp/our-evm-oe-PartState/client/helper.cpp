#include "helper.h"

//////////////////// AUX ////////////////////
auto sep = separator{" "};

const std::string expand_var(const std::string token_text, std::unordered_map<std::string, std::string>& sh_vars)
{
    if (token_text.substr(0, 1) == "$" && sh_vars.end() != sh_vars.find(token_text)) {
        return sh_vars[token_text];
    } else {
        return token_text;
    }
}

std::string expand_vars(const char* command, std::unordered_map<std::string, std::string>& sh_vars)
{
    auto sep = separator{" \t"};
    auto tmp = std::string(command);
    auto tokens = boost::tokenizer<separator>{tmp, sep};

    std::string ret("");

    // iterate over all parameters of the requested endpoint
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        // debug_print(fmt::format("token: {}", *it));
        ret.append(expand_var(*it, sh_vars));
        ret.append(" ");
    }
    return ret;  // RVO
}

bool correct_token_cnt(std::string& command, std::set<unsigned> allowedCnts, boost::tokenizer<separator>** tokens, uint* cnt = NULL)
{
    *tokens = new boost::tokenizer<separator>{command, sep};
    auto _tokens = *tokens;

    auto distance = std::distance(_tokens->begin(), _tokens->end());
    if (allowedCnts.end() == allowedCnts.find(distance)) {
        std::cerr << "wrong token count: " << std::distance(_tokens->begin(), _tokens->end()) << "\n";
        return false;
    }
    if (NULL != cnt)
        *cnt = distance;
    return true;
}
//////////////////// AUX ////////////////////
