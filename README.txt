

                           Rails - An IDA Pro Plugin

                         Copyright (c) 2012, Dean Pucsek

                              dean@lightbulbone.com



------ 1. OVERVIEW ------

Rails is a plugin for the IDA Pro disassembler.  It enables you to easily work
with multiple binaries will keeping each IDA Pro database isolated.  With Rails
you can view comments for functions in other open instances of IDA Pro, jump
directly to functions in other instances, and easily navigate between instances
of IDA Pro.


------ 2. LICENSE ------

This plugin is availbe under the BSD 3-clause license, see LICENSE.txt 
for details.


------ 3. BUILD & INSTALL ------

Note: Currently only Mac OS X is supported.

To build and install the Rails plugin you must do the following.

   1. Clone the source from the Git repository.

      git clone git@github.com:lightbulbone/rails.git

   2. Edit the top-level Makefile to point to your Qt libraries and
      the IDA Pro SDK.

   3. Run 'make' and 'make install'

This will build the plugin and install it into the IDA Pro plugins directory.


------ 4. USAGE ------

There are three primary functions in Rails surrounding comments and navigation
between binaries.

Begin by opening up several instances of IDA Pro and activating Rails in each.
You will see the Rails window appear, this shows a list of linked instances
along with an area to view comments and other relevant messages.

To view comments for an external function all you have to do is highlight
the function you want to get comments for and then go to Edit->Rails - Comments
in the menu.  Alternatively, the default hotkey for this action is Alt-c.  This
will cause the comment to be printed int he message area.

Navigating to an external function is done by highlighting the function name
you wish to jump to and then going to Edit->Rails - Jump.  The hotkey for this
action is Alt-j.  Navigating to an external function will cause the owning 
instance to come to the front and adjust its view so the function is centered
within it.

Finally, you can jump between instances by doubling clicking the binary name
in the list of linked instances.  This will result in the desired instance
coming to the front and becoming active.
