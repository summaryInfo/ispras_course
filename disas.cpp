#include "vm.hpp"
#include "util.hpp"


void disas_object(const object_file &obj, const char *file) {
    /* That looks like a hack... */
    if (file && !std::freopen(file, "r", stdin)) {
        std::cerr << "Cannot open file '" << file << "'" << std::endl;
        throw std::invalid_argument("No file");
    } else file = "<stdin>";

    for (const auto &gl : obj.globals) {
        std::cout << ".global " << 

    }

}

int main(int argc, char *argv[]) {

    if ((argc > 1 && !std::strcmp("-h", argv[1])) || argc < 2) {
        std::cout << "Usage:\n" << std::endl;
        std::cout << "\t" << argv[0] << " <infile> [<outfile>]" << std::endl;
        std::cout << "Default value of <outfile> is stdin" << std::endl;
        return 0;
    }

    std::ifstream str(argv[1], std::ios::binary);
    object_file obj;
    obj.read(str);

    disas_object(obj, argc > 2 ? argv[2] : nullptr);

    return 0;
}
