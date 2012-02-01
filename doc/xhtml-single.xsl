<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>
    <xsl:param name="section.autolabel" select="'1'"/>
    <xsl:param name="make.valid.html" select="'1'"/>
    <xsl:param name="use.id.as.filename" select="'1'"/>
    <xsl:param name="img.src.path" select="'images/'"/>
    <xsl:param name="section.label.includes.component.label" select="'1'"/>
    <xsl:param name="funcsynopsis.style">ansi</xsl:param>
    <xsl:param name="table.borders.with.css" select="'1'"/>
    <xsl:param name="html.stylesheet" select="'clogs.css'"/>
    <xsl:param name="html.stylesheet.type" select="'text/css'"/>
    <xsl:template match="/book/bookinfo/releaseinfo">
        <releaseinfo>
            <xsl:value-of select="$clogs.version"/>
        </releaseinfo>
    </xsl:template>
</xsl:stylesheet>
