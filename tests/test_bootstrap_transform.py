#!/usr/bin/env python3
import unittest
from tools.bootstrap_deps import pe_machine, transform_xdbot, validate_xdbot_addobject


class BootstrapTransformTests(unittest.TestCase):
    def test_xdbot_hook_wrapper_is_removed_but_logic_suffix_is_exact(self):
        sample_hpp = (
            '#pragma once\n#include "../includes.hpp"\n'
            'const std::string newColors = "x";\n'
            'const std::unordered_set<int> robtopLevelIDs = { 1 };\n'
            'const std::unordered_set<int> excludedTriggerIDs = { 1, 2 };\n'
            'const std::map<int, std::vector<int>> importantTriggerIDs = { { 1, { 2 } } };\n'
            'const std::unordered_set<int> decoObjectIDs = { 3, 4 };\n'
            'const std::unordered_set<int> solidObjectIDs = { 5, 6 };\n'
        )
        logic = (
            'std::string LayoutMode::getModifiedString(std::string levelString){\n'
            'auto decompString = ZipUtils::decompressString(levelString.c_str(), true, 0);\n'
            "auto objectStrings = Utils::splitByChar(decompString, ';');\n"
            'std::vector<std::string> levelSettings; levelSettings[1] = newColors;\n'
            'std::map<int,std::string> props;\n'
            'if (importantTriggerIDs.contains(1)) {}\n'
            'if (decoObjectIDs.contains(1) || props.contains(121)) {}\n'
            'if (props.contains(57)) {}\n'
            'if (solidObjectIDs.contains(1)) {}\n'
            'if (props.contains(129)) {}\n'
            'if (props.contains(135)) {}\n'
            'std::string firstPart, newString; return firstPart + newString + ";";}\n'
            'std::string LayoutMode::mergeVector(std::vector<std::string> v,std::string s){\n'
            'std::string result; return result;}\n'
        )
        sample_cpp = '#include "layout_mode.hpp"\nclass $modify(X){};\n' + logic

        hpp, cpp, pristine_logic = transform_xdbot(sample_hpp, sample_cpp)
        self.assertIn('xdbot_compat.hpp', hpp)
        self.assertNotIn('../includes.hpp', hpp)
        self.assertNotIn('$modify', cpp)
        self.assertEqual(pristine_logic, logic)
        self.assertEqual(cpp, '#include "layout_mode.hpp"\n\n' + logic)

    def test_pinned_addobject_behavior_exists_in_mirror_hook(self):
        raw_cpp = r'''
void addObject(GameObject * obj) {
    if (!Global::get().layoutMode) return PlayLayer::addObject(obj);
    if (excludedTriggerIDs.contains(obj->m_objectID)) return;
    PlayLayer::addObject(obj);
    obj->m_activeMainColorID = -1;
    obj->m_activeDetailColorID = -1;
    obj->m_detailUsesHSV = false;
    obj->m_baseUsesHSV = false;
    obj->m_hasNoGlow = true;
    obj->m_isHide = obj->m_objectID == 2065;
    obj->setOpacity(obj->m_objectID == 2065 ? 0 : 255);
    obj->setVisible(obj->m_objectID != 2065);
}
'''
        validate_xdbot_addobject(raw_cpp)

    def test_truncated_xdbot_cpp_is_rejected_without_size_heuristics(self):
        sample_hpp = (
            '#pragma once\n#include "../includes.hpp"\n'
            'const std::string newColors = "x";\n'
            'const std::unordered_set<int> robtopLevelIDs = { 1 };\n'
            'const std::unordered_set<int> excludedTriggerIDs = { 1 };\n'
            'const std::map<int, std::vector<int>> importantTriggerIDs = { { 1, { 2 } } };\n'
            'const std::unordered_set<int> decoObjectIDs = { 3 };\n'
            'const std::unordered_set<int> solidObjectIDs = { 4 };\n'
        )
        truncated_cpp = (
            'class $modify(X){};\n'
            'std::string LayoutMode::getModifiedString(std::string levelString){\n'
            'auto decompString = ZipUtils::decompressString(levelString.c_str(), true, 0);\n'
            "auto objectStrings = Utils::splitByChar(decompString, ';');\n"
            'std::vector<std::string> levelSettings; levelSettings[1] = newColors;\n'
            'std::map<int,std::string> props;\n'
            'if (importantTriggerIDs.contains(1)) {}\n'
            'if (decoObjectIDs.contains(1) || props.contains(121)) {}\n'
            'if (props.contains(57)) {}\n'
            'if (solidObjectIDs.contains(1)) {}\n'
            'if (props.contains(129)) {}\n'
            'if (props.contains(135)) {}\n'
            'std::string firstPart, newString; return firstPart + newString + ";";}\n'
            'std::string LayoutMode::mergeVector(std::vector<std::string> v,std::string s){'
        )
        with self.assertRaisesRegex(RuntimeError, "truncated"):
            transform_xdbot(sample_hpp, truncated_cpp)

    def test_pe_machine_detects_amd64(self):
        data = bytearray(256)
        data[:2] = b"MZ"
        data[0x3C:0x40] = (0x80).to_bytes(4, "little")
        data[0x80:0x84] = b"PE\0\0"
        data[0x84:0x86] = (0x8664).to_bytes(2, "little")
        self.assertEqual(pe_machine(bytes(data)), 0x8664)


if __name__ == '__main__':
    unittest.main()
