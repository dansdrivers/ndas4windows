<?xml version="1.0" encoding="utf-8" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output method="text" encoding="unicode"/>

<!-- 

XSL for MC (Message Compiler) 

Copyright (C) 2004-2004 XIMETA, Inc.

Last Updated: February 2005, Chesong Lee

Note:

 Output mc file is a unicode format. Run mc with -u parameter to compile.
 > mc -u file.mc 
 
Parameter: langs

 You can filter output languages using "langs" parameter.

 If parameter is not set, default is emit messages in all languages.
 To include certain languages only, set this parameter to
 comma(or semicolon) separated list of language codes.

 For example:

 This parameter declaration or input (from xsl processor) will include
 only messages in NEU language.

  <xsl:param name="langs">NEU</xsl:param>

 This will include NEU, KOR and JPN.

  <xsl:param name="langs">NEU;KOR;JPN</xsl:param>

 Parameter: tag
 
  Tag parameter uses alternative text for tagged text if such text exists.
  
-->
<xsl:param name="langs"></xsl:param>
<xsl:param name="tag"></xsl:param>

<xsl:template match="/mc"><xsl:value-of select="preamble/text()" />
<xsl:apply-templates select="messageId" />
<xsl:apply-templates select="severities" />
<xsl:apply-templates select="facilities" />
<xsl:apply-templates select="languages" />
<xsl:apply-templates select="messages" />
<xsl:value-of select="postscript/text()" />
</xsl:template>

<xsl:template match="messageId">
MessageIdTypeDef=<xsl:value-of select="@type" />
<xsl:text>&#13;&#10;</xsl:text>
</xsl:template>

<xsl:template match="severities">
SeverityNames=(
<xsl:apply-templates select="severity" />)
</xsl:template>

<xsl:template match="severity">
<xsl:value-of select="@name" />=<xsl:value-of select="@value" />:<xsl:value-of select="@code" />
<xsl:text>&#13;&#10;</xsl:text>
</xsl:template>
<xsl:template match="facilities">
FacilityNames=(
<xsl:apply-templates select="facility" />)
</xsl:template>

<xsl:template match="facility">
<xsl:value-of select="@name" />=<xsl:value-of select="@value" />
<xsl:text>&#13;&#10;</xsl:text>
</xsl:template>

<xsl:template match="languages">
LanguageNames=(
<xsl:apply-templates select="language" />)
</xsl:template>

<xsl:template match="language">
<xsl:value-of select="@name" />=<xsl:value-of select="@value" />:<xsl:value-of select="@output" />
<xsl:text>&#13;&#10;</xsl:text>
</xsl:template>

<xsl:template match="messages">
	<!-- <xsl:if test="@module=$module and @langid=$lang" > -->
		<xsl:apply-templates select="message"/>
	<!-- </xsl:if> -->
</xsl:template>

<xsl:template match="message">
MessageId=<xsl:value-of select="@id" />
SymbolicName=<xsl:value-of select="@symbolicName" />
Severity=<xsl:value-of select="@severity" />
Facility=<xsl:value-of select="@facility" />

<xsl:variable name="neu-text">
<xsl:choose>
	<xsl:when test="$tag and text[@lang='NEU'][@tag=$tag][text() and text()!='-']">
		<xsl:value-of select="text[@lang='NEU'][@tag=$tag]" />
	</xsl:when>
	<xsl:otherwise>
		<xsl:value-of select="text[@lang='NEU']" />
	</xsl:otherwise>
</xsl:choose>
</xsl:variable>

<xsl:for-each select="text[not(@tag)]">

<xsl:if test="not($langs) or contains($langs,@lang)">
Language=<xsl:value-of select="@lang" />
<xsl:text>&#13;&#10;</xsl:text>
<!-- Tagging Support -->
<xsl:variable name="curlang" select="@lang" />
<xsl:variable name="tagged" select="../text[@tag=$tag][@lang=$curlang]/text()" />
<xsl:choose>
	<xsl:when test="$tagged">
		<xsl:choose>
			<xsl:when test="text()='-'"><xsl:value-of select="$neu-text" /></xsl:when>
			<xsl:when test="not(text())"><xsl:value-of select="$neu-text" /></xsl:when>
			<xsl:otherwise><xsl:value-of select="$tagged" /></xsl:otherwise>
		</xsl:choose>
	</xsl:when>
	<xsl:otherwise>
		<xsl:choose>
			<xsl:when test="text()='-'"><xsl:value-of select="$neu-text" /></xsl:when>
			<xsl:when test="not(text())"><xsl:value-of select="$neu-text" /></xsl:when>
			<xsl:otherwise><xsl:value-of select="text()"/></xsl:otherwise>
		</xsl:choose>
	</xsl:otherwise>
</xsl:choose>
<xsl:text>&#13;&#10;.</xsl:text>

</xsl:if>
</xsl:for-each>
</xsl:template>

</xsl:stylesheet>

<!--

A sample mc file:

======= BEGIN SAMPLE =======
MessageIdTypeDef=DWORD

SeverityNames=(
Success=0x0:STATUS_SEVERITY_SUCCESS
Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
Warning=0x2:STATUS_SEVERITY_WARNING
Error=0x3:STATUS_SEVERITY_ERROR
)

FacilityNames=(
NDASUSER=0x001
NDASCOMM=0x002
NDASOP=0x003
NDASPATCH=0x004
NDASSVC=0x010
NDASDM=0x110
EVT_NDASSVC=0xF00
)

LanguageNames=(
NEU=0:ndasmsg_neu
ENU=0x409:ndasmsg_enu
CHS=0x804:ndasmsg_chs
DEU=0x407:ndasmsg_deu
ESN=0xC0A:ndasmsg_esn
FRA=0x40C:ndasmsg_fra
ITA=0x410:ndasmsg_ita
JPN=0x411:ndasmsg_jpn
KOR=0x412:ndasmsg_kor
PTG=0x816:ndasmsg_ptg
)

MessageId=0x021
SymbolicName=NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND
Severity=Error
Facility=NDASSVC
Language=NEU
HD-HBGLU2 Device Entry Not Found.
.
MessageId=0x022
SymbolicName=NDASSVC_ERROR_UNITDEVICE_ENTRY_NOT_FOUND
Severity=Error
Facility=NDASSVC
Language=NEU
HD-HBGLU2 Unit Device Entry Not Found.
.

======= END SAMPLE =======

-->
