/* 
 */

/*

    Copyright (C) 2014 Ferrero Andrea

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.


 */

/*

    These files are distributed with PhotoFlow - http://aferrero2707.github.io/PhotoFlow/

 */

#include <iostream>

#include "imagebutton.hh"


PF::ImageButton::ImageButton(Glib::ustring i, Glib::ustring pi)
{
  img.set( i );
  pressed_img.set( pi );

  img_align.set(0.5,0.5,0,0);
  pressed_img_align.set(0.5,0.5,0,0);

  img_align.add(img);
  pressed_img_align.add(pressed_img);

  button_box.pack_start( img_align, Gtk::PACK_EXPAND_WIDGET );
  //button_box.pack_start( pressed_img, Gtk::PACK_SHRINK );
  event_box.add( button_box );
  event_box.add_events( Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK );

  pack_start( event_box, Gtk::PACK_EXPAND_WIDGET );

  show_all_children();

  //pressed_img.hide();
}


void PF::ImageButton::on_realize()
{
  //img.show();
  //pressed_img.hide();
  Gtk::VBox::on_realize();
}


void PF::ImageButton::on_map()
{
  //img.show();
  //pressed_img.hide();
  Gtk::VBox::on_map();
}


bool PF::ImageButton::on_button_press_event( GdkEventButton* button )
{
  if( button->type != GDK_BUTTON_PRESS || button->button != 1 ) return false;
#ifndef NDEBUG
  std::cout<<"PF::ToggleImageButton::on_button_press_event(): button "<<button->button<<" pressed."<<std::endl;
#endif
  button_box.remove( img_align );
  button_box.pack_start( pressed_img_align, Gtk::PACK_SHRINK );
  show_all_children();
  return true;
}


bool PF::ImageButton::on_button_release_event( GdkEventButton* button )
{
#ifndef NDEBUG
  std::cout<<"PF::ToggleImageButton::on_button_release_event(): button "<<button->button<<" released."<<std::endl;
#endif
  if( button->button != 1 ) return false;

  signal_clicked.emit();

  button_box.remove( pressed_img_align );
  button_box.pack_start( img_align, Gtk::PACK_SHRINK );
  show_all_children();
  return true;
}



PF::ToggleImageButton::ToggleImageButton(Glib::ustring active, Glib::ustring inactive,
    bool t, bool a):
active(true),
do_toggle(t),
pressed(false)
{
  active_img.set( active );
  inactive_img.set( inactive );
  button_box.pack_start( active_img, Gtk::PACK_SHRINK );
  event_box.add( button_box );
  event_box.add_events( Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK );

  pack_start( event_box, Gtk::PACK_SHRINK );

  //event_box.signal_button_release_event().connect(sigc::mem_fun(*this,
  //    &ToggleImageButton::on_button_release_event) );

  set_active(a);
  show_all_children();
}



void PF::ToggleImageButton::set_active( bool newval )
{
  //std::cout<<"ToggleImageButton::set_active("<<newval<<") called (active="<<active<<")"<<std::endl;
  if( active == newval ) return;
  if( inactive_img.get_parent() == &button_box )
    button_box.remove( inactive_img );
  if( active_img.get_parent() == &button_box )
    button_box.remove( active_img );
  if( newval ) {
    button_box.pack_start( active_img, Gtk::PACK_SHRINK );
    active_img.show();
  } else {
    button_box.pack_start( inactive_img, Gtk::PACK_SHRINK );
    inactive_img.show();
  }
  active = newval;
}



void PF::ToggleImageButton::toggle()
{
  bool new_state = !is_active();
  //std::cout<<"ToggleImageButton::toggle(): is_active()="<<is_active()<<"  new_state="<<new_state<<std::endl;
  set_active( new_state );
  if( new_state ) {
    signal_activated.emit();
  } else {
    signal_deactivated.emit();
  }
}



bool PF::ToggleImageButton::on_button_press_event( GdkEventButton* button )
{
#ifndef NDEBUG
  std::cout<<"PF::ToggleImageButton::on_button_press_event(): button "<<button->button<<" pressed."<<std::endl;
#endif
  if( button->button != 1 ) return false;
  pressed = true;
  signal_pressed.emit();
  return true;
}


bool PF::ToggleImageButton::on_button_release_event( GdkEventButton* button )
{
#ifndef NDEBUG
  std::cout<<"PF::ToggleImageButton::on_button_release_event(): button "<<button->button<<" released."<<std::endl;
#endif
  if( button->button != 1 ) return false;

  pressed = false;

  if( do_toggle ) {
    toggle();
  } else {
    if( active ) signal_clicked.emit();
  }
  return true;
}


void PF::ToggleImageButtonsBox::add_button( PF::ToggleImageButton* button )
{
  buttons.push_back(button);
  pack_start( *button, Gtk::PACK_SHRINK, 4 );

  button->signal_pressed.connect(sigc::mem_fun(*this,
          &ToggleImageButtonsBox::on_button_pressed) );
}


void PF::ToggleImageButtonsBox::on_button_pressed()
{
  for( unsigned int i = 0; i < buttons.size(); i++ ) {
    if( buttons[i]->is_active() ) {
      if( !(buttons[i]->is_pressed()) ) {
        // de-activate the button, except if it is the pressed one
        buttons[i]->toggle();
      }
    } else {
      if( buttons[i]->is_pressed() ) {
        // if it is the pressed one, activate it
        buttons[i]->toggle();
      }
    }
  }
}


