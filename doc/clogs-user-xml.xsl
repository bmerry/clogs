<?xml version="1.0" encoding="UTF-8"?>
<!--
   Copyright (c) 2012 University of Cape Town

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>
    <xsl:param name="section.autolabel" select="'1'"/>
    <xsl:param name="make.valid.html" select="'1'"/>
    <xsl:param name="use.id.as.filename" select="'1'"/>
    <xsl:param name="img.src.path" select="'images/'"/>
    <xsl:param name="section.label.includes.component.label" select="'1'"/>
    <xsl:param name="funcsynopsis.style">ansi</xsl:param>
    <xsl:param name="table.borders.with.css" select="'1'"/>
    <xsl:param name="html.stylesheet" select="'clogs-user.css'"/>
    <xsl:param name="html.stylesheet.type" select="'text/css'"/>
    <xsl:param name="use.svg" select="1"/>
    <xsl:template match="/book/bookinfo/releaseinfo">
        <releaseinfo>
            <xsl:value-of select="$clogs.version"/>
        </releaseinfo>
    </xsl:template>
</xsl:stylesheet>
