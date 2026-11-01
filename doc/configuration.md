# The configuration file
Synnical is a System manager as you know; and a chunk of it uses toml/toml++, because
of this you would assume that its configuration file uses toml. Yet it does not use toml
and instead uses INI as it is lightweight, while being very easy and simple to understand.
The configuration file lives at: `~/.config/synnical/settings.ini`

## Getting started
The configuration file for Synnical does not have a ton of options unlike other Package Managers/
System Managers. It has options for colors, being verbose, the amount jobs for compilation, etc.
The config has three sections/headers, `[general]`, `[build]`, and `[download]`. 

An example configuration at `~/.config/synnical/settings.ini`:

```ini
[general]
color = always
verbose = false

[build]
jobs = 8
show_output = false

[download]
progress = true
style = trail
```
It currently does not support more than one progress bar/style, but I will add more.

## Options and their meaning

#### General section
`[general]`, the general section is the main section which holds two options: `color`, and `verbose`. 
which handles whether you want colors or not, and if you want full compiler output, if you decide to not turn on verbose than
you will get the filtered output. The filtered output tends to be a lot nicer if you don't
want your terminal being flooded with build and configure output. This option is a bool which means `true` or `false`.

#### Build section
The `[build]` section holds the `jobs` option, the jobs option holds the amount of jobs used in compilation.
If you dont know what jobs are, jobs determine how many processors the cpu uses to compile
something. The higher the jobs the faster the compilation, but that comes at the cost of
higher and louder fan speeds. The build section also holds the `show_output` option, this
option determines whether or not you want any build or configure output. It is also a bool
aka `true` or `false`.

#### Download section
The `[download]` section holds two settings, `progress` and `style`. The progress setting
is a bool, it decides whether or not Synnical shows the download bar and progress of downloading. 
The `style` setting decides the progress bar style, there is 2 options currently, trail and hash. 
The default option is trail which uses a unicode bar and arrow. And hash is a bar made of hashtags such as: [#################]. 

That is all the configuration options that our ini configuration file holds.
