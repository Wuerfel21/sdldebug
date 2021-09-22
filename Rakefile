# encoding: utf-8
# frozen_string_literal: true

require "rake/clean"
require 'rake/loaders/makefile'
Rake.application.add_loader("d", Rake::MakefileLoader.new)

FileList["*.d"].each{|f| import f} # import depfiles

def windows?
    Gem.win_platform?
end

CPP_COMPILER = "g++"
CPP_OPTS = "-Og -Wall -g"
LINK_LIBS = "#{"-lmingw32" if windows?} -lSDL2main -lSDL2 -lSDL2_ttf"

rule ".o" => ".cpp" do |t|
    sh "#{CPP_COMPILER} #{CPP_OPTS} -MMD -c #{t.source} --std=c++17"
end

file "p2debug.exe" => FileList["*.cpp"].pathmap('%X.o') do |t|
    sh "#{CPP_COMPILER} #{t.sources.join ' '} #{LINK_LIBS} -o #{t.name}"
end

task :default => "p2debug.exe"

rule ".binary" => ".spin2" do |t|
    sh "flexspin -2 -gbrk #{t.source}"
end

task :examples => FileList["example/*.spin2"].pathmap('%X.binary')

CLEAN.include %w[*.o *.d example/*.binary example/*.p2asm]
CLOBBER.include %w[p2debug.exe]

