#include <fstream>
#include <string>

// read the html in filename and return a string
std::string read_html(const char *fileName) {
  std::ifstream ifs;
  std::string html_code = "";

  ifs.open(fileName, std::ifstream::in);
  char c = ifs.get();

  while (ifs.good()) {
    html_code += c;
    c = ifs.get();
  }

  ifs.close();

  return html_code;
}