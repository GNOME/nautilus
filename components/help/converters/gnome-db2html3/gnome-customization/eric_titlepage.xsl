<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- This stylesheet was created by template/titlepage.xsl; do not edit it by hand. -->


  

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.recto">
    
  <xsl:choose xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:when xmlns:xsl="http://www.w3.org/1999/XSL/Transform" test="bookinfo/title">
      <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/title"/>
    </xsl:when>
    <xsl:when xmlns:xsl="http://www.w3.org/1999/XSL/Transform" test="title">
      <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="title"/>
    </xsl:when>
  </xsl:choose>

    
  <xsl:choose xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:when xmlns:xsl="http://www.w3.org/1999/XSL/Transform" test="bookinfo/subtitle">
      <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/subtitle"/>
    </xsl:when>
    <xsl:when xmlns:xsl="http://www.w3.org/1999/XSL/Transform" test="subtitle">
      <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="subtitle"/>
    </xsl:when>
  </xsl:choose>

    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/corpauthor"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/authorgroup"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/author"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/othercredit"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/copyright"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/legalnotice"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/releaseinfo"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/pubdate"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/revision"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/revhistory"/>
    
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" mode="book.titlepage.recto.auto.mode" select="bookinfo/abstract"/>
  
</xsl:template>
    
    
    
    
    
    
    
    
    
    
    
    
    
  

  

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.verso">
  
</xsl:template>
  

  

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.separator">
    <hr/>
  
</xsl:template>

  

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.before.recto">
    <img align="right" src="stylesheet/gnome-logo-large.png" alt="GNOME Logo"/>
  
</xsl:template>

  

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.before.verso">
  
</xsl:template>


<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage">
  <div class="titlepage">
    <xsl:call-template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.before.recto"/>
    <xsl:call-template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.recto"/>
    <xsl:call-template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.before.verso"/>
    <xsl:call-template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.verso"/>
    <xsl:call-template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" name="book.titlepage.separator"/>
  </div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="*" mode="book.titlepage.recto.mode">
  <!-- if an element isn't found in this mode, -->
  <!-- try the generic titlepage.mode -->
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="titlepage.mode"/>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="*" mode="book.titlepage.verso.mode">
  <!-- if an element isn't found in this mode, -->
  <!-- try the generic titlepage.mode -->
  <xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="titlepage.mode"/>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="title" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="subtitle" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="corpauthor" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="authorgroup" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="author" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="othercredit" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="copyright" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="legalnotice" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="releaseinfo" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="pubdate" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="revision" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="revhistory" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>

<xsl:template xmlns:xsl="http://www.w3.org/1999/XSL/Transform" match="abstract" mode="book.titlepage.recto.auto.mode">
<div use-attribute-sets="book.titlepage.recto.style">
<xsl:apply-templates xmlns:xsl="http://www.w3.org/1999/XSL/Transform" select="." mode="book.titlepage.recto.mode"/>
</div>
</xsl:template>


</xsl:stylesheet>
