#!/usr/bin/env python3
"""
Add ui_framework target to mapview.xcodeproj
This script safely modifies the Xcode project file to add a static library target for ui_framework.
"""

import sys
import uuid
import re

def generate_xcode_id():
    """Generate a unique 24-character hex ID for Xcode objects"""
    return uuid.uuid4().hex[:24].upper()

def add_ui_framework_target(project_file):
    """Add ui_framework static library target to Xcode project"""
    
    with open(project_file, 'r') as f:
        content = f.read()
    
    # Generate unique IDs for new objects
    ui_framework_target_id = generate_xcode_id()
    ui_framework_group_id = generate_xcode_id()
    ui_framework_lib_id = generate_xcode_id()
    ui_framework_sources_id = generate_xcode_id()
    ui_framework_headers_id = generate_xcode_id()
    ui_framework_frameworks_id = generate_xcode_id()
    ui_framework_buildconfig_debug_id = generate_xcode_id()
    ui_framework_buildconfig_release_id = generate_xcode_id()
    ui_framework_configlist_id = generate_xcode_id()
    
    # 1. Add PBXFileReference for libui_framework.a
    file_ref_section = content.find('/* End PBXFileReference section */')
    if file_ref_section == -1:
        print("Error: Could not find PBXFileReference section")
        return False
    
    new_file_ref = f'\t\t{ui_framework_lib_id} /* libui_framework.a */ = {{isa = PBXFileReference; explicitFileType = archive.ar; includeInIndex = 0; path = libui_framework.a; sourceTree = BUILT_PRODUCTS_DIR; }};\n'
    content = content[:file_ref_section] + new_file_ref + content[file_ref_section:]
    
    # 2. Add PBXFileSystemSynchronizedRootGroup for ui_framework
    root_group_section = content.find('/* End PBXFileSystemSynchronizedRootGroup section */')
    if root_group_section == -1:
        print("Error: Could not find PBXFileSystemSynchronizedRootGroup section")
        return False
    
    new_root_group = f'''\t\t{ui_framework_group_id} /* ui_framework */ = {{
\t\t\tisa = PBXFileSystemSynchronizedRootGroup;
\t\t\tpath = ui_framework;
\t\t\tsourceTree = "<group>";
\t\t}};
'''
    content = content[:root_group_section] + new_root_group + content[root_group_section:]
    
    # 3. Add PBXFrameworksBuildPhase for ui_framework
    frameworks_section = content.find('/* End PBXFrameworksBuildPhase section */')
    if frameworks_section == -1:
        print("Error: Could not find PBXFrameworksBuildPhase section")
        return False
    
    new_frameworks = f'''\t\t{ui_framework_frameworks_id} /* Frameworks */ = {{
\t\t\tisa = PBXFrameworksBuildPhase;
\t\t\tbuildActionMask = 2147483647;
\t\t\tfiles = (
\t\t\t);
\t\t\trunOnlyForDeploymentPostprocessing = 0;
\t\t}};
'''
    content = content[:frameworks_section] + new_frameworks + content[frameworks_section:]
    
    # 4. Add ui_framework to main PBXGroup children
    main_group_match = re.search(r'(785D12A02DB8DDF300567DFE = \{[^}]+children = \()[^)]+(\);)', content)
    if main_group_match:
        children_list = main_group_match.group(0)
        # Add ui_framework reference after hexen
        new_children = children_list.replace(
            '7815CD892DCE0E20000617A1 /* hexen */,',
            f'7815CD892DCE0E20000617A1 /* hexen */,\n\t\t\t\t{ui_framework_group_id} /* ui_framework */,'
        )
        content = content.replace(children_list, new_children)
    
    # 5. Add libui_framework.a to Products
    products_match = re.search(r'(785D12AA2DB8DDF300567DFE /\* Products \*/ = \{[^}]+children = \()[^)]+(\);)', content)
    if products_match:
        products_list = products_match.group(0)
        new_products = products_list.replace(
            '7815CD662DCE0DC7000617A1 /* libhexen.dylib */,',
            f'7815CD662DCE0DC7000617A1 /* libhexen.dylib */,\n\t\t\t\t{ui_framework_lib_id} /* libui_framework.a */,'
        )
        content = content.replace(products_list, new_products)
    
    # 6. Add PBXHeadersBuildPhase
    headers_section = content.find('/* End PBXHeadersBuildPhase section */')
    if headers_section == -1:
        print("Error: Could not find PBXHeadersBuildPhase section")
        return False
    
    new_headers = f'''\t\t{ui_framework_headers_id} /* Headers */ = {{
\t\t\tisa = PBXHeadersBuildPhase;
\t\t\tbuildActionMask = 2147483647;
\t\t\tfiles = (
\t\t\t);
\t\t\trunOnlyForDeploymentPostprocessing = 0;
\t\t}};
'''
    content = content[:headers_section] + new_headers + content[headers_section:]
    
    # 7. Add PBXNativeTarget for ui_framework
    native_target_section = content.find('/* End PBXNativeTarget section */')
    if native_target_section == -1:
        print("Error: Could not find PBXNativeTarget section")
        return False
    
    new_target = f'''\t\t{ui_framework_target_id} /* ui_framework */ = {{
\t\t\tisa = PBXNativeTarget;
\t\t\tbuildConfigurationList = {ui_framework_configlist_id} /* Build configuration list for PBXNativeTarget "ui_framework" */;
\t\t\tbuildPhases = (
\t\t\t\t{ui_framework_headers_id} /* Headers */,
\t\t\t\t{ui_framework_sources_id} /* Sources */,
\t\t\t\t{ui_framework_frameworks_id} /* Frameworks */,
\t\t\t);
\t\t\tbuildRules = (
\t\t\t);
\t\t\tdependencies = (
\t\t\t);
\t\t\tfileSystemSynchronizedGroups = (
\t\t\t\t{ui_framework_group_id} /* ui_framework */,
\t\t\t);
\t\t\tname = ui_framework;
\t\t\tpackageProductDependencies = (
\t\t\t);
\t\t\tproductName = ui_framework;
\t\t\tproductReference = {ui_framework_lib_id} /* libui_framework.a */;
\t\t\tproductType = "com.apple.product-type.library.static";
\t\t}};
'''
    content = content[:native_target_section] + new_target + content[native_target_section:]
    
    # 8. Add PBXSourcesBuildPhase
    sources_section = content.find('/* End PBXSourcesBuildPhase section */')
    if sources_section == -1:
        print("Error: Could not find PBXSourcesBuildPhase section")
        return False
    
    new_sources = f'''\t\t{ui_framework_sources_id} /* Sources */ = {{
\t\t\tisa = PBXSourcesBuildPhase;
\t\t\tbuildActionMask = 2147483647;
\t\t\tfiles = (
\t\t\t);
\t\t\trunOnlyForDeploymentPostprocessing = 0;
\t\t}};
'''
    content = content[:sources_section] + new_sources + content[sources_section:]
    
    # 9. Add ui_framework to project targets list
    project_targets_match = re.search(r'(projectRoot = "";[^}]+targets = \()[^)]+(\);)', content)
    if project_targets_match:
        targets_list = project_targets_match.group(0)
        new_targets = targets_list.replace(
            '7815CD5F2DCE0DC7000617A1 /* hexen */,',
            f'7815CD5F2DCE0DC7000617A1 /* hexen */,\n\t\t\t\t{ui_framework_target_id} /* ui_framework */,'
        )
        content = content.replace(targets_list, new_targets)
    
    # 10. Add XCBuildConfiguration for Debug and Release
    build_config_section = content.find('/* End XCBuildConfiguration section */')
    if build_config_section == -1:
        print("Error: Could not find XCBuildConfiguration section")
        return False
    
    new_debug_config = f'''\t\t{ui_framework_buildconfig_debug_id} /* Debug */ = {{
\t\t\tisa = XCBuildConfiguration;
\t\t\tbuildSettings = {{
\t\t\t\tCODE_SIGN_STYLE = Automatic;
\t\t\t\tDEVELOPMENT_TEAM = BM2R8F5YHC;
\t\t\t\tEXECUTABLE_PREFIX = lib;
\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (
\t\t\t\t\t"DEBUG=1",
\t\t\t\t\t"$(inherited)",
\t\t\t\t);
\t\t\t\tHEADER_SEARCH_PATHS = (
\t\t\t\t\t"$(inherited)",
\t\t\t\t\t"$(SRCROOT)/ui_framework",
\t\t\t\t);
\t\t\t\tPRODUCT_NAME = "$(TARGET_NAME)";
\t\t\t\tSKIP_INSTALL = YES;
\t\t\t}};
\t\t\tname = Debug;
\t\t}};
\t\t{ui_framework_buildconfig_release_id} /* Release */ = {{
\t\t\tisa = XCBuildConfiguration;
\t\t\tbuildSettings = {{
\t\t\t\tCODE_SIGN_STYLE = Automatic;
\t\t\t\tDEVELOPMENT_TEAM = BM2R8F5YHC;
\t\t\t\tEXECUTABLE_PREFIX = lib;
\t\t\t\tHEADER_SEARCH_PATHS = (
\t\t\t\t\t"$(inherited)",
\t\t\t\t\t"$(SRCROOT)/ui_framework",
\t\t\t\t);
\t\t\t\tPRODUCT_NAME = "$(TARGET_NAME)";
\t\t\t\tSKIP_INSTALL = YES;
\t\t\t}};
\t\t\tname = Release;
\t\t}};
'''
    content = content[:build_config_section] + new_debug_config + content[build_config_section:]
    
    # 11. Add XCConfigurationList
    config_list_section = content.find('/* End XCConfigurationList section */')
    if config_list_section == -1:
        print("Error: Could not find XCConfigurationList section")
        return False
    
    new_config_list = f'''\t\t{ui_framework_configlist_id} /* Build configuration list for PBXNativeTarget "ui_framework" */ = {{
\t\t\tisa = XCConfigurationList;
\t\t\tbuildConfigurations = (
\t\t\t\t{ui_framework_buildconfig_debug_id} /* Debug */,
\t\t\t\t{ui_framework_buildconfig_release_id} /* Release */,
\t\t\t);
\t\t\tdefaultConfigurationIsVisible = 0;
\t\t\tdefaultConfigurationName = Release;
\t\t}};
'''
    content = content[:config_list_section] + new_config_list + content[config_list_section:]
    
    # Write back the modified content
    with open(project_file, 'w') as f:
        f.write(content)
    
    print("✅ Successfully added ui_framework target to Xcode project")
    print(f"   Target ID: {ui_framework_target_id}")
    print(f"   Product: libui_framework.a")
    return True

if __name__ == '__main__':
    project_file = 'mapview.xcodeproj/project.pbxproj'
    
    print("Adding ui_framework target to Xcode project...")
    if add_ui_framework_target(project_file):
        print("\n✅ Done! You can now open the project in Xcode and build the ui_framework target.")
    else:
        print("\n❌ Failed to add target. Please check error messages above.")
        sys.exit(1)
