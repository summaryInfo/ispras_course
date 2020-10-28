#include "vm.hpp"

uint32_t object_file::id(std::string &&name) {
    auto idx = strtab.find(name);
    if (idx != strtab.end()) {
        return idx->second;
    } else {
        size_t pos = strtab_offset;
        strtab_offset += name.length() + 1;
        strtab.emplace(std::move(name), pos);
        return pos;
    }
}

void object_file::swap(object_file &other) {
    std::swap(strtab, other.strtab);
    std::swap(strtab_offset, other.strtab_offset);
    std::swap(global_indices, other.global_indices);
    std::swap(globals, other.globals);
    std::swap(function_indices, other.function_indices);
    std::swap(functions, other.functions);
}

void object_file::write(std::ostream &st) {
    /* 1. file header */
    vm_header hd = {
        /* magic */{'X','S','V','M'},
        /* flags */ 0,
        /* functab_offset */ sizeof(vm_header),
        /* functab_size */ static_cast<uint32_t>(sizeof(vm_function)*functions.size()),
        /* globtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size()),
        /* globtab_size */ static_cast<uint32_t>(sizeof(vm_global)*globals.size()),
        /* strtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size() + sizeof(vm_global)*globals.size()),
        /* strtab_size */ static_cast<uint32_t>(strtab_offset)
    };
    size_t file_offset = hd.strtab_offset + hd.strtab_size;

    st.write(reinterpret_cast<char *>(&hd), sizeof(hd));

    /* 2. function table */
    for (auto &f : functions) {
        vm_function vmf = {
            f.name,
            f.signature,
            static_cast<uint32_t>(file_offset),
            static_cast<uint32_t>(f.code.size())
        };
        st.write(reinterpret_cast<char *>(&vmf), sizeof(vmf));
        file_offset += f.code.size();
    }

    /* 3. globals table */
    st.write(reinterpret_cast<char *>(&globals), globals.size()*sizeof(vm_global));

    /* 4. string table */
    std::map<uint32_t, std::string> revmap;
    for (auto &i : strtab) revmap.emplace(i.second, i.first);
    for (auto &str : revmap) st.write(str.second.data(), str.second.size() + 1);

    /* 5. functions code */
    for (auto &f : functions) st.write(reinterpret_cast<char *>(f.code.data()), f.code.size());

    st.flush();
}

object_file object_file::read(std::istream &str) {

}

