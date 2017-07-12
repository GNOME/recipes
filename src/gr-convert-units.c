/* gr-convert-units.c:
 *
 * Copyright (C) 2017 Paxana Xander <paxana@paxana.me>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include "gr-settings.h"
#include <stdlib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <langinfo.h>
#include <gtk/gtk.h>
#include "gr-unit.h"
#include "gr-convert-units.h"
#include <math.h>

int
get_temperature_unit (void)
{
        int unit;
        GSettings *settings = gr_settings_get ();

        unit = g_settings_get_enum (settings, "temperature-unit");

        if (unit == GR_TEMPERATURE_UNIT_LOCALE) {
#ifdef LC_MEASUREMENT
                const char *fmt;

                fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
                if (fmt && *fmt == 2)
                        unit = GR_TEMPERATURE_UNIT_FAHRENHEIT;
                else
#endif
                        unit = GR_TEMPERATURE_UNIT_CELSIUS;
        }

        return unit;
}

static int
get_volume_unit (void)
{
        int unit;
        GSettings *settings = gr_settings_get ();

        unit = g_settings_get_enum (settings, "volume-unit");

        if (unit == GR_VOLUME_UNIT_LOCALE) {
#ifdef LC_MEASUREMENT
                const char *fmt;

                fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
                if (fmt && *fmt == 2)
                        unit = GR_VOLUME_UNIT_IMPERIAL;
                else
#endif
                        unit = GR_VOLUME_UNIT_METRIC;
        }

        return unit;
}

static int
get_weight_unit (void)
{
        int unit;
        GSettings *settings = gr_settings_get ();

        unit = g_settings_get_enum (settings, "weight-unit");

        if (unit == GR_VOLUME_UNIT_LOCALE) {
#ifdef LC_MEASUREMENT
                const char *fmt;

                fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
                if (fmt && *fmt == 2)
                        unit = GR_VOLUME_UNIT_IMPERIAL;
                else
#endif
                        unit = GR_VOLUME_UNIT_METRIC;
        }

        return unit;
}


void
convert_temp (int *num, int *unit, int user_unit)
{
    int num1 = *num;
    int unit1 = *unit;
                
        if (unit1 == GR_TEMPERATURE_UNIT_CELSIUS &&
                user_unit == GR_TEMPERATURE_UNIT_FAHRENHEIT) {
                    num1 = (num1 * 1.8) + 32;
                    unit1 = user_unit;
                }

        else if (unit1 == GR_TEMPERATURE_UNIT_FAHRENHEIT &&
                user_unit == GR_TEMPERATURE_UNIT_CELSIUS) {
                    num1 = (num1 - 32) / 1.8;
                    unit1 = user_unit;
                }
                                
                *unit = unit1;
                *num = num1;
}

void 
convert_volume (double *amount, char **unit)
{
        double amount1 = *amount;        
        char *unit1 = *unit;

        int user_volume_unit = get_volume_unit();

                if (user_volume_unit == GR_VOLUME_UNIT_IMPERIAL) {
                        
                       if (strcmp(unit1, "ml") == 0)
                                {
                                        amount1 = (amount1 / 4.92892);
                                        unit1 = "tsp";
                                }
                        else if (strcmp(unit1, "dl") == 0)
                                {
                                        amount1 = (amount1 * 20.2884);
                                        unit1 = "tsp";
                                }
                        else if (strcmp(unit1, "l") == 0)
                        {
                                amount1 = (amount1 * 202.884);
                                unit1 = "tsp";
                        }
        }
                if (user_volume_unit == GR_VOLUME_UNIT_METRIC) {
                       if (strcmp(unit1, "tsp") == 0)
                                {
                                        amount1 = (amount1 * 4.92892);
                                        unit1 = "ml";
                                }
                        else if (strcmp(unit1, "tbsp") == 0)
                                {
                                        amount1 = (amount1 * 14.79);
                                        unit1 = "ml";
                                }
                        else if (strcmp(unit1, "cup") == 0)
                        {
                                amount1 = (amount1 * 236.59);
                                unit1 = "ml";
                        }
                        else if (strcmp(unit1, "pt") == 0)
                                {
                                amount1 = (amount1 * 473.176);
                                unit1 = "ml";
                                }
                        else if (strcmp(unit1, "qt") == 0)
                        {
                                amount1 = (amount1 * 946.353);
                                unit1 = "ml";
                        }
                        else if (strcmp(unit1, "gal") == 0)
                        {
                                amount1 = (amount1 * 3785.41);
                                unit1 = "ml";
                        }
                        else if (strcmp(unit1, "fl oz") == 0)
                        {
                                amount1 = (amount1 * 29.5735);
                                unit1 = "ml";
                        }
                        else if (strcmp(unit1, "fl. oz.") == 0)
                        {
                                amount1 = (amount1 * 29.5735);
                                unit1 = "ml";
                        }
        }
                                *amount = amount1;
                                *unit = unit1;
}

void 
convert_weight (double *amount, char **unit)
{
        double amount1 = *amount;        
        char *unit1 = *unit;

        int user_weight_unit = get_weight_unit();

        if (user_weight_unit == GR_VOLUME_UNIT_IMPERIAL) {

                        if (strcmp(unit1, "g") == 0)
                        {
                                amount1 = (amount1 * 0.035274);
                                unit1 = "oz";   
                        }
                        else if (strcmp(unit1, "kg") == 0)
                        {
                                amount1 = (amount1 * 35.274);
                                unit1 = "oz";
                        }
                
        }
        
        if (user_weight_unit == GR_VOLUME_UNIT_METRIC) {

                       if (strcmp(unit1, "oz") == 0)
                        {
                                amount1 = (amount1 * 28.3495);
                                unit1 = "g";
                        }
                        else if (strcmp(unit1, "lb") == 0)
                        {
                                amount1 = (amount1 * 453.592);
                                unit1 = "g";
                        }
                        else if (strcmp(unit1, "st") == 0)
                        {
                                amount1 = (amount1 * 6350.29);
                                unit1 = "g";
                        } 
        }
                *amount = amount1;
                *unit = unit1;
}

void 
human_readable (double *amount, char **unit)
{
        double amount1 = *amount;        
        char *unit1 = *unit;

        int id = gr_unit_get_id_number(*unit);
        g_message("UNIT is %s", unit1);
        g_message("ID is %i", id);
        g_message("amount is %f", amount1);

        switch (id) {
                case GR_UNIT_GRAM: 
                        if (amount1 >= 1000) {
                                amount1 = (amount1 / 1000);
                                unit1 = "kg";  
                        }
                break;

                case GR_UNIT_OUNCE:
                        if (amount1 >= 16) {
                                amount1 = (amount1 / 16);
                                unit1 = "lb";  
                        }
                break;

                case GR_UNIT_TEASPOON:
                        if (amount1 >= 3) {
                                amount1 = (amount1 / 3);
                                unit1 = "tbsp";     
                        }
                break;

                case GR_UNIT_TABLESPOON:
                        if (amount1 >= 16) {
                                amount1 = (amount1 / 16);
                                unit1 = "cup";
                        }
                        
                        else if (amount1 < 3) {
                                g_message("Here I am");
                                amount1 = (amount1 * 3);
                                unit1 = "tsp";
                        }
                        
                break;
                
                case GR_UNIT_CUP:
                        if (amount1 >= 4) {
                                amount1 = (amount1 / 4);
                                unit1 = "qt";
                        }
                        
                        else if (amount1 < 1) {
                                amount1 = amount1 / 16;
                                unit1 = "tbsp";
                        }
                break;

                default:
                g_message("default"); 
        }
        
        *amount = amount1;
        *unit = unit1;
}

/*
        if ((strcmp(unit1, "g") == 0) && (amount1 >= 1000) )
        {
                amount1 = (amount1 / 1000);
                unit1 = "kg";
        }
        if ((strcmp(unit1, "oz") == 0) && (amount1 >= 16) )
        {
                amount1 = (amount1 / 16);
                unit1 = "lb";
        } 
        if ((strcmp(unit1, "tsp") == 0) && (amount1 >= 3) )
        {
                amount1 = (amount1 / 3);
                unit1 = "tbsp";
        }
        if ((strcmp(unit1, "tbsp") == 0) && (amount1 >= 16) )
        {
                amount1 = (amount1 / 16);
                unit1 = "cup";
        }
        if ((strcmp(unit1, "cup") == 0) && (amount1 >= 4) )
        {
                amount1 = (amount1 / 4);
                unit1 = "qt";
        }
        if ((strcmp(unit1, "qt") == 0) && (amount1 >= 4) )
        {
                amount1 = (amount1 / 4);
                unit1 = "gal";
        }
        if ((strcmp(unit1, "ml") == 0) && (amount1 >= 1000) )
        {
                amount1 = (amount1 / 1000);
                unit1 = "l";
                }

        *amount = amount1;
        *unit = unit1;

}
        */
void 
round_amount (double *amount, char **unit)
{
        double amount1 = *amount;        
        char *unit1 = *unit;

                double integral;
                double fractional;

                g_message("amount before is %f", amount1);
                fractional = modf(amount1, &integral);

                printf("%f\n", integral);
                printf("%f\n", fractional);

                if (fractional > .5) {
                        double difference = (1 - fractional);
                        amount1 = (amount1 + difference);
                }
                if (fractional < .5 )
                {
                        double difference = (1 - fractional);
                        amount1 = (amount1 - difference); 
                }

                g_message("amount after is %f", amount1);

                *amount = amount1;
                *unit = unit1;
}