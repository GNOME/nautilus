<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                exclude-result-prefixes="#default">
<!--########################Files to Include######################-->

<!-- Importing the Norman Walsh's stylesheet as the basis. -->
<xsl:import href="/usr/share/sgml/docbook/stylesheet/xsl/nwalsh-1.45/html/chunk.xsl"/>

<!-- Including a new title page stylesheet for customizing the placement 
	 of tags in the title page. -->
<xsl:include href="eric_titlepage.xsl"/>
<!--##################Variables and Parameters####################-->

<xsl:param name="use.id.as.filename" select="1" doc:type='boolean'/>

<xsl:param name="make.year.ranges" select="1" doc:type="boolean"/>

<!--**************************TOC*********************************-->

<!-- Should books have a TOC? 0 or 1 -->
<xsl:param name="generate.book.toc" select="1" doc:type="boolean"/>
<!-- Should articles have a TOC? 0 or 1 -->
<xsl:param name="generate.article.toc" select="1" doc:type="boolean"/>
<!-- Should parts have a TOC? 0 or 1 -->
<xsl:param name="generate.part.toc" select="1" doc:type="boolean"/>

<!-- Should chapters be labeled? 0 or 1 -->
<xsl:param name="chapter.autolabel" select="1" doc:type="boolean"/>

<!-- Should sections be labeled? 0 or 1 -->
<xsl:param name="section.autolabel" select="1" doc:type ="boolean"/>

<!-- Related to section labels, should those labels include the chapter
     number in them (i.e., 1.1, 1.2, 1.3, 1.4 ) -->
<xsl:param name="section.label.includes.component.label" select="1" doc:type="boolean"/>

<!-- Sets the number of sub-sections displayed in the section TOC -->
<xsl:param name="toc.section.depth" select="1"/>

<!--********************************Admonitions*******************-->

<!-- Should graphics be included in admonitions? 0 or 1 -->
<xsl:param name="admon.graphics" select="1"/>

<!-- Specifies the default path for admonition graphics -->
<xsl:param name="admon.graphics.path">./stylesheet/</xsl:param>

<!-- Specifies the default graphic file if none is given. -->
<xsl:param name="graphic.default.extension" select="'.png'" doc:type="string"/>


<!-- ###############################Templates#####################-->

<!-- Custom template to make the emphasis tag bold the text instead of 
	 the text be italicized. -->
<xsl:template match="emphasis">
  <xsl:call-template name="inline.boldseq"/>
</xsl:template>

<!-- Custom template to make the term tag bold.-->
<xsl:template match="variablelist//term">
  <xsl:call-template name="inline.boldseq"/>
</xsl:template>

<!-- Custom template to make the releaseinfo tag italicized. -->
<xsl:template match="releaseinfo" mode="titlepage.mode">
  <p class="{name(.)}"><i>
  <xsl:apply-templates mode="titlepage.mode"/>
  </i></p>
</xsl:template>

<!-- Moves the title just to the right of the admonition graphic. -->
<xsl:template name="graphical.admonition">
  <div class="{name(.)}">
  <xsl:if test="$admon.style != ''">
    <xsl:attribute name="style">
      <xsl:value-of select="$admon.style"/>
    </xsl:attribute>
  </xsl:if>
  <table border="0">
    <tr>
      <td rowspan="2" align="center" valign="top">
        <xsl:attribute name="width">
          <xsl:call-template name="admon.graphic.width"/>
        </xsl:attribute>
        <img>
          <xsl:attribute name="src">
            <xsl:call-template name="admon.graphic"/>
          </xsl:attribute>
        </img>
      </td>
      <th align="left" valign="top">
        <xsl:call-template name="anchor"/>
        <xsl:apply-templates select="." mode="object.title.markup"/>
      </th>
    </tr>
    <tr>
      <td colspan="2" align="left" valign="top">
        <xsl:apply-templates/>
      </td>
    </tr>
  </table>
  </div>
</xsl:template>

<!-- Adds the string "Written By:" to the author tag in the 
	 title page.  Also adds a maintainer by the role attribute 
	 to the author tag so the program maintainer can be specified 
	 and the application name in the condition attribute. -->
<xsl:variable name="authorlabel">Written by: </xsl:variable>
<xsl:variable name="maintainlabel">maintained by: </xsl:variable>

<xsl:template match="author" mode="titlepage.mode">
  <h3 class="{name(.)}">
  
  <xsl:choose>
	<xsl:when test="@role='maintainer'">
      <xsl:if test="string-length(@condition)">
	    <xsl:value-of select="@condition"/>
	    <xsl:text> is </xsl:text>
      </xsl:if>	  
	  <xsl:value-of select="$maintainlabel"/>
      <xsl:call-template name="person.name"/>
      <xsl:value-of select="\<\/h3\>"/>
      <xsl:apply-templates mode="titlepage.mode" select="./affiliation"/>
	</xsl:when>
    <xsl:otherwise>
	  <xsl:value-of select="$authorlabel"/>
      <xsl:call-template name="person.name"/>
	  </h3>
	  <apply-templates mode="titlepage.mode select="./affiliation"/>
      <xsl:text>,</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  
</xsl:template>

<xsl:template match="author[position()=last()]">
  <xsl:apply-templates/>
</xsl:template>

<!-- Makes the address inside the legalnotice inline instead of separated 
  	 from the body of the text.-->
<xsl:template match="legalnotice//address">
  <xsl:param name="suppress-numbers" select="'0'"/>
  <xsl:variable name="vendor" select="system-property('xsl:vendor')"/>

  <xsl:variable name="rtf">
    <xsl:apply-templates mode="inline-address"/>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$suppress-numbers = '0'
                    and @linenumbering = 'numbered'
                    and $use.extensions != '0'
                    and $linenumbering.extension != '0'">
          <xsl:call-template name="number.rtf.lines">
            <xsl:with-param name="rtf" select="$rtf"/>
          </xsl:call-template>
    </xsl:when>

    <xsl:otherwise>
          <xsl:apply-templates mode="inline-address"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="legalnotice">
  <xsl:text>&#xA;</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<!-- Added a comma after the street, city and postal code. -->
<xsl:template match="street|city|postcode" mode="inline-address">
  <xsl:apply-templates mode="inline-address"/>
  <xsl:text>, </xsl:text>
</xsl:template>

<xsl:template match="state|country" mode="inline-address">   
  <xsl:apply-templates mode="inline-address"/>
</xsl:template>

</xsl:stylesheet>






