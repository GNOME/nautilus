<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<xsl:import href="../xsl-stylesheets-1.29/html/chunk-common.xsl"/>
<xsl:import href="../xsl-stylesheets-1.29/html/docbook.xsl"/>

<xsl:param name="gdb_docname" />

<xsl:param name="gdb_pathname" />

<xsl:output encoding="ISO-8859-1" />

<!-- don't render the legalnotice on the titlepage -->
<xsl:template match="legalnotice " mode="titlepage.mode">
</xsl:template>


<!-- customize the copyright notice to be a link to the legalnotice -->
<xsl:template match="copyright" mode="titlepage.mode">
  <xsl:variable name="years" select="year"/>
  <xsl:variable name="holders" select="holder"/>

  <p class="{name(.)}">
  <a>
    <xsl:attribute name="href">
    <xsl:value-of select="$gdb_docname"/>
    <xsl:text>?legalnotice</xsl:text>
    </xsl:attribute>
    <xsl:call-template name="gentext.element.name"/>
    <xsl:call-template name="gentext.space"/>
    <xsl:call-template name="dingbat">
      <xsl:with-param name="dingbat">copyright</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="gentext.space"/>
    <xsl:apply-templates select="$years" mode="titlepage.mode"/>
    <xsl:call-template name="gentext.space"/>
    <xsl:call-template name="gentext.by"/>
    <xsl:call-template name="gentext.space"/>
    <xsl:apply-templates select="$holders" mode="titlepage.mode"/>
  </a>
  </p>
</xsl:template>

<xsl:template match="releaseinfo" mode="titlepage.mode">
  <span class="{name(.)}">
    <xsl:apply-templates mode="titlepage.mode"/>
    <br/>
  </span>
</xsl:template>

<!--
<xsl:template match="graphic">
  <p>
    <img>
    <xsl:attribute name="src">
    <xsl:text>file://</xsl:text>
    <xsl:value-of select="$gdb_pathname"/>
    <xsl:text>/figures/example_panel.png</xsl:text>
    </xsl:attribute>
    </img>
  </p>
</xsl:template>
-->

<xsl:template name="process.image">
  <!-- When this template is called, the current node should be  -->
  <!-- a graphic, inlinegraphic, imagedata, or videodata. All    -->
  <!-- those elements have the same set of attributes, so we can -->
  <!-- handle them all in one place.                             -->
  <xsl:param name="tag" select="'img'"/>
  <xsl:param name="alt"/>

  <xsl:variable name="filename">
    <xsl:choose>
      <xsl:when test="local-name(.) = 'graphic'
                      or local-name(.) = 'inlinegraphic'">
        <xsl:choose>
          <xsl:when test="@fileref">
            <xsl:value-of select="@fileref"/>
          </xsl:when>
          <xsl:when test="@entityref">
            <xsl:value-of select="unparsed-entity-uri(@entityref)"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message>
              <xsl:text>A fileref or entityref is required on </xsl:text>
              <xsl:value-of select="local-name(.)"/>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <!-- imagedata, videodata, audiodata -->
        <xsl:call-template name="mediaobject.filename">
          <xsl:with-param name="object" select=".."/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="width">
    <xsl:choose>
      <xsl:when test="@scale"><xsl:value-of select="@scale"/>%</xsl:when>
      <xsl:when test="@width"><xsl:value-of select="@width"/></xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="height">
    <xsl:choose>
      <xsl:when test="@scale"></xsl:when>
      <xsl:when test="@depth"><xsl:value-of select="@depth"/></xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="align">
    <xsl:value-of select="@align"/>
  </xsl:variable>

  <xsl:element name="{$tag}">
    <xsl:attribute name="src">

 <xsl:text>file://</xsl:text>
    <xsl:value-of select="$gdb_pathname"/>
    <xsl:text>/</xsl:text>
      <xsl:value-of select="$filename"/>
	<xsl:text>.png</xsl:text>
    </xsl:attribute>

    <xsl:if test="$align != ''">
      <xsl:attribute name="align">
        <xsl:value-of select="$align"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$height != ''">
      <xsl:attribute name="height">
        <xsl:value-of select="$height"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$width != ''">
      <xsl:attribute name="width">
        <xsl:value-of select="$width"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$alt != ''">
      <xsl:attribute name="alt">
        <xsl:value-of select="$alt"/>
      </xsl:attribute>
    </xsl:if>
  </xsl:element>
</xsl:template>

</xsl:stylesheet>