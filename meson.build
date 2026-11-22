project(
    'Vulkan', 'cpp',
    version : '1.0.0',
    default_options : ['warning_level=3', 'cpp_std=c++20']
)

add_project_arguments('-mmacosx-version-min=27.2', language : 'cpp')
add_project_link_arguments('-mmacosx-version-min=27.2', language : 'cpp')

volk_inc = include_directories('external/volk')
vma_inc  = include_directories('external/vma')

sdl3_dep    = dependency('sdl3')
vulkan_dep  = dependency('vulkan')
shaderc_dep = dependency('shaderc')

Vulkan = executable(
    'Vulkan',
    sources : [
        'src/main.cpp',
        'src/application.h',
        'src/application.cpp',
        'src/vma_impl.cpp'
    ],
    include_directories : [volk_inc, vma_inc],
    dependencies : [sdl3_dep, vulkan_dep, shaderc_dep],
    install : true
)

test('test', Vulkan)